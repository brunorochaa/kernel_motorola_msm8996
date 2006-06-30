/*
 * Copyright (C) 2004 Intel Corporation <naveen.b.s@intel.com>
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * ACPI based HotPlug driver that supports Memory Hotplug
 * This driver fields notifications from firmare for memory add
 * and remove operations and alerts the VM of the affected memory
 * ranges.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/memory_hotplug.h>
#include <acpi/acpi_drivers.h>

#define ACPI_MEMORY_DEVICE_COMPONENT		0x08000000UL
#define ACPI_MEMORY_DEVICE_CLASS		"memory"
#define ACPI_MEMORY_DEVICE_HID			"PNP0C80"
#define ACPI_MEMORY_DEVICE_DRIVER_NAME		"Hotplug Mem Driver"
#define ACPI_MEMORY_DEVICE_NAME			"Hotplug Mem Device"

#define _COMPONENT		ACPI_MEMORY_DEVICE_COMPONENT

ACPI_MODULE_NAME("acpi_memory")
    MODULE_AUTHOR("Naveen B S <naveen.b.s@intel.com>");
MODULE_DESCRIPTION(ACPI_MEMORY_DEVICE_DRIVER_NAME);
MODULE_LICENSE("GPL");

/* ACPI _STA method values */
#define ACPI_MEMORY_STA_PRESENT		(0x00000001UL)
#define ACPI_MEMORY_STA_ENABLED		(0x00000002UL)
#define ACPI_MEMORY_STA_FUNCTIONAL	(0x00000008UL)

/* Memory Device States */
#define MEMORY_INVALID_STATE	0
#define MEMORY_POWER_ON_STATE	1
#define MEMORY_POWER_OFF_STATE	2

static int acpi_memory_device_add(struct acpi_device *device);
static int acpi_memory_device_remove(struct acpi_device *device, int type);
static int acpi_memory_device_start(struct acpi_device *device);

static struct acpi_driver acpi_memory_device_driver = {
	.name = ACPI_MEMORY_DEVICE_DRIVER_NAME,
	.class = ACPI_MEMORY_DEVICE_CLASS,
	.ids = ACPI_MEMORY_DEVICE_HID,
	.ops = {
		.add = acpi_memory_device_add,
		.remove = acpi_memory_device_remove,
		.start = acpi_memory_device_start,
		},
};

struct acpi_memory_info {
	struct list_head list;
	u64 start_addr;		/* Memory Range start physical addr */
	u64 length;		/* Memory Range length */
	unsigned short caching;	/* memory cache attribute */
	unsigned short write_protect;	/* memory read/write attribute */
	unsigned int enabled:1;
};

struct acpi_memory_device {
	acpi_handle handle;
	unsigned int state;	/* State of the memory device */
	struct list_head res_list;
};

static acpi_status
acpi_memory_get_resource(struct acpi_resource *resource, void *context)
{
	struct acpi_memory_device *mem_device = context;
	struct acpi_resource_address64 address64;
	struct acpi_memory_info *info, *new;
	acpi_status status;

	status = acpi_resource_to_address64(resource, &address64);
	if (ACPI_FAILURE(status) ||
	    (address64.resource_type != ACPI_MEMORY_RANGE))
		return AE_OK;

	list_for_each_entry(info, &mem_device->res_list, list) {
		/* Can we combine the resource range information? */
		if ((info->caching == address64.info.mem.caching) &&
		    (info->write_protect == address64.info.mem.write_protect) &&
		    (info->start_addr + info->length == address64.minimum)) {
			info->length += address64.address_length;
			return AE_OK;
		}
	}

	new = kzalloc(sizeof(struct acpi_memory_info), GFP_KERNEL);
	if (!new)
		return AE_ERROR;

	INIT_LIST_HEAD(&new->list);
	new->caching = address64.info.mem.caching;
	new->write_protect = address64.info.mem.write_protect;
	new->start_addr = address64.minimum;
	new->length = address64.address_length;
	list_add_tail(&new->list, &mem_device->res_list);

	return AE_OK;
}

static int
acpi_memory_get_device_resources(struct acpi_memory_device *mem_device)
{
	acpi_status status;
	struct acpi_memory_info *info, *n;


	status = acpi_walk_resources(mem_device->handle, METHOD_NAME__CRS,
				     acpi_memory_get_resource, mem_device);
	if (ACPI_FAILURE(status)) {
		list_for_each_entry_safe(info, n, &mem_device->res_list, list)
			kfree(info);
		return -EINVAL;
	}

	return 0;
}

static int
acpi_memory_get_device(acpi_handle handle,
		       struct acpi_memory_device **mem_device)
{
	acpi_status status;
	acpi_handle phandle;
	struct acpi_device *device = NULL;
	struct acpi_device *pdevice = NULL;


	if (!acpi_bus_get_device(handle, &device) && device)
		goto end;

	status = acpi_get_parent(handle, &phandle);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "Cannot find acpi parent"));
		return -EINVAL;
	}

	/* Get the parent device */
	status = acpi_bus_get_device(phandle, &pdevice);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "Cannot get acpi bus device"));
		return -EINVAL;
	}

	/*
	 * Now add the notified device.  This creates the acpi_device
	 * and invokes .add function
	 */
	status = acpi_bus_add(&device, pdevice, handle, ACPI_BUS_TYPE_DEVICE);
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "Cannot add acpi bus"));
		return -EINVAL;
	}

      end:
	*mem_device = acpi_driver_data(device);
	if (!(*mem_device)) {
		printk(KERN_ERR "\n driver data not found");
		return -ENODEV;
	}

	return 0;
}

static int acpi_memory_check_device(struct acpi_memory_device *mem_device)
{
	unsigned long current_status;


	/* Get device present/absent information from the _STA */
	if (ACPI_FAILURE(acpi_evaluate_integer(mem_device->handle, "_STA",
					       NULL, &current_status)))
		return -ENODEV;
	/*
	 * Check for device status. Device should be
	 * present/enabled/functioning.
	 */
	if (!((current_status & ACPI_MEMORY_STA_PRESENT)
	      && (current_status & ACPI_MEMORY_STA_ENABLED)
	      && (current_status & ACPI_MEMORY_STA_FUNCTIONAL)))
		return -ENODEV;

	return 0;
}

static int acpi_memory_enable_device(struct acpi_memory_device *mem_device)
{
	int result, num_enabled = 0;
	struct acpi_memory_info *info;
	int node;


	/* Get the range from the _CRS */
	result = acpi_memory_get_device_resources(mem_device);
	if (result) {
		printk(KERN_ERR PREFIX "get_device_resources failed\n");
		mem_device->state = MEMORY_INVALID_STATE;
		return result;
	}

	node = acpi_get_node(mem_device->handle);
	/*
	 * Tell the VM there is more memory here...
	 * Note: Assume that this function returns zero on success
	 * We don't have memory-hot-add rollback function,now.
	 * (i.e. memory-hot-remove function)
	 */
	list_for_each_entry(info, &mem_device->res_list, list) {
		u64 start_pfn, end_pfn;

		start_pfn = info->start_addr >> PAGE_SHIFT;
		end_pfn = (info->start_addr + info->length - 1) >> PAGE_SHIFT;

		if (pfn_valid(start_pfn) || pfn_valid(end_pfn)) {
			/* already enabled. try next area */
			num_enabled++;
			continue;
		}

		result = add_memory(node, info->start_addr, info->length);
		if (result)
			continue;
		info->enabled = 1;
		num_enabled++;
	}
	if (!num_enabled) {
		printk(KERN_ERR PREFIX "add_memory failed\n");
		mem_device->state = MEMORY_INVALID_STATE;
		return -EINVAL;
	}

	return result;
}

static int acpi_memory_powerdown_device(struct acpi_memory_device *mem_device)
{
	acpi_status status;
	struct acpi_object_list arg_list;
	union acpi_object arg;
	unsigned long current_status;


	/* Issue the _EJ0 command */
	arg_list.count = 1;
	arg_list.pointer = &arg;
	arg.type = ACPI_TYPE_INTEGER;
	arg.integer.value = 1;
	status = acpi_evaluate_object(mem_device->handle,
				      "_EJ0", &arg_list, NULL);
	/* Return on _EJ0 failure */
	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "_EJ0 failed"));
		return -ENODEV;
	}

	/* Evalute _STA to check if the device is disabled */
	status = acpi_evaluate_integer(mem_device->handle, "_STA",
				       NULL, &current_status);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	/* Check for device status.  Device should be disabled */
	if (current_status & ACPI_MEMORY_STA_ENABLED)
		return -EINVAL;

	return 0;
}

static int acpi_memory_disable_device(struct acpi_memory_device *mem_device)
{
	int result;
	struct acpi_memory_info *info, *n;


	/*
	 * Ask the VM to offline this memory range.
	 * Note: Assume that this function returns zero on success
	 */
	list_for_each_entry_safe(info, n, &mem_device->res_list, list) {
		if (info->enabled) {
			result = remove_memory(info->start_addr, info->length);
			if (result)
				return result;
		}
		kfree(info);
	}

	/* Power-off and eject the device */
	result = acpi_memory_powerdown_device(mem_device);
	if (result) {
		/* Set the status of the device to invalid */
		mem_device->state = MEMORY_INVALID_STATE;
		return result;
	}

	mem_device->state = MEMORY_POWER_OFF_STATE;
	return result;
}

static void acpi_memory_device_notify(acpi_handle handle, u32 event, void *data)
{
	struct acpi_memory_device *mem_device;
	struct acpi_device *device;


	switch (event) {
	case ACPI_NOTIFY_BUS_CHECK:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "\nReceived BUS CHECK notification for device\n"));
		/* Fall Through */
	case ACPI_NOTIFY_DEVICE_CHECK:
		if (event == ACPI_NOTIFY_DEVICE_CHECK)
			ACPI_DEBUG_PRINT((ACPI_DB_INFO,
					  "\nReceived DEVICE CHECK notification for device\n"));
		if (acpi_memory_get_device(handle, &mem_device)) {
			printk(KERN_ERR PREFIX "Cannot find driver data\n");
			return;
		}

		if (!acpi_memory_check_device(mem_device)) {
			if (acpi_memory_enable_device(mem_device))
				printk(KERN_ERR PREFIX
					    "Cannot enable memory device\n");
		}
		break;
	case ACPI_NOTIFY_EJECT_REQUEST:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "\nReceived EJECT REQUEST notification for device\n"));

		if (acpi_bus_get_device(handle, &device)) {
			printk(KERN_ERR PREFIX "Device doesn't exist\n");
			break;
		}
		mem_device = acpi_driver_data(device);
		if (!mem_device) {
			printk(KERN_ERR PREFIX "Driver Data is NULL\n");
			break;
		}

		/*
		 * Currently disabling memory device from kernel mode
		 * TBD: Can also be disabled from user mode scripts
		 * TBD: Can also be disabled by Callback registration
		 *      with generic sysfs driver
		 */
		if (acpi_memory_disable_device(mem_device))
			printk(KERN_ERR PREFIX
				    "Disable memory device\n");
		/*
		 * TBD: Invoke acpi_bus_remove to cleanup data structures
		 */
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
				  "Unsupported event [0x%x]\n", event));
		break;
	}

	return;
}

static int acpi_memory_device_add(struct acpi_device *device)
{
	int result;
	struct acpi_memory_device *mem_device = NULL;


	if (!device)
		return -EINVAL;

	mem_device = kmalloc(sizeof(struct acpi_memory_device), GFP_KERNEL);
	if (!mem_device)
		return -ENOMEM;
	memset(mem_device, 0, sizeof(struct acpi_memory_device));

	INIT_LIST_HEAD(&mem_device->res_list);
	mem_device->handle = device->handle;
	sprintf(acpi_device_name(device), "%s", ACPI_MEMORY_DEVICE_NAME);
	sprintf(acpi_device_class(device), "%s", ACPI_MEMORY_DEVICE_CLASS);
	acpi_driver_data(device) = mem_device;

	/* Get the range from the _CRS */
	result = acpi_memory_get_device_resources(mem_device);
	if (result) {
		kfree(mem_device);
		return result;
	}

	/* Set the device state */
	mem_device->state = MEMORY_POWER_ON_STATE;

	printk(KERN_INFO "%s \n", acpi_device_name(device));

	return result;
}

static int acpi_memory_device_remove(struct acpi_device *device, int type)
{
	struct acpi_memory_device *mem_device = NULL;


	if (!device || !acpi_driver_data(device))
		return -EINVAL;

	mem_device = (struct acpi_memory_device *)acpi_driver_data(device);
	kfree(mem_device);

	return 0;
}

static int acpi_memory_device_start (struct acpi_device *device)
{
	struct acpi_memory_device *mem_device;
	int result = 0;

	mem_device = acpi_driver_data(device);

	if (!acpi_memory_check_device(mem_device)) {
		/* call add_memory func */
		result = acpi_memory_enable_device(mem_device);
		if (result)
			ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				"Error in acpi_memory_enable_device\n"));
	}
	return result;
}

/*
 * Helper function to check for memory device
 */
static acpi_status is_memory_device(acpi_handle handle)
{
	char *hardware_id;
	acpi_status status;
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	struct acpi_device_info *info;


	status = acpi_get_object_info(handle, &buffer);
	if (ACPI_FAILURE(status))
		return status;

	info = buffer.pointer;
	if (!(info->valid & ACPI_VALID_HID)) {
		acpi_os_free(buffer.pointer);
		return AE_ERROR;
	}

	hardware_id = info->hardware_id.value;
	if ((hardware_id == NULL) ||
	    (strcmp(hardware_id, ACPI_MEMORY_DEVICE_HID)))
		status = AE_ERROR;

	acpi_os_free(buffer.pointer);
	return status;
}

static acpi_status
acpi_memory_register_notify_handler(acpi_handle handle,
				    u32 level, void *ctxt, void **retv)
{
	acpi_status status;


	status = is_memory_device(handle);
	if (ACPI_FAILURE(status)){
		ACPI_EXCEPTION((AE_INFO, status, "handle is no memory device"));
		return AE_OK;	/* continue */
	}

	status = acpi_install_notify_handler(handle, ACPI_SYSTEM_NOTIFY,
					     acpi_memory_device_notify, NULL);
	/* continue */
	return AE_OK;
}

static acpi_status
acpi_memory_deregister_notify_handler(acpi_handle handle,
				      u32 level, void *ctxt, void **retv)
{
	acpi_status status;


	status = is_memory_device(handle);
	if (ACPI_FAILURE(status)){
		ACPI_EXCEPTION((AE_INFO, status, "handle is no memory device"));
		return AE_OK;	/* continue */
	}

	status = acpi_remove_notify_handler(handle,
					    ACPI_SYSTEM_NOTIFY,
					    acpi_memory_device_notify);

	return AE_OK;	/* continue */
}

static int __init acpi_memory_device_init(void)
{
	int result;
	acpi_status status;


	result = acpi_bus_register_driver(&acpi_memory_device_driver);

	if (result < 0)
		return -ENODEV;

	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				     ACPI_UINT32_MAX,
				     acpi_memory_register_notify_handler,
				     NULL, NULL);

	if (ACPI_FAILURE(status)) {
		ACPI_EXCEPTION((AE_INFO, status, "walk_namespace failed"));
		acpi_bus_unregister_driver(&acpi_memory_device_driver);
		return -ENODEV;
	}

	return 0;
}

static void __exit acpi_memory_device_exit(void)
{
	acpi_status status;


	/*
	 * Adding this to un-install notification handlers for all the device
	 * handles.
	 */
	status = acpi_walk_namespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
				     ACPI_UINT32_MAX,
				     acpi_memory_deregister_notify_handler,
				     NULL, NULL);

	if (ACPI_FAILURE(status))
		ACPI_EXCEPTION((AE_INFO, status, "walk_namespace failed"));

	acpi_bus_unregister_driver(&acpi_memory_device_driver);

	return;
}

module_init(acpi_memory_device_init);
module_exit(acpi_memory_device_exit);
