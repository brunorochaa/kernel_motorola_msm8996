/*
 * Marvell Wireless LAN device driver: station event handling
 *
 * Copyright (C) 2011, Marvell International Ltd.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 */

#include "decl.h"
#include "ioctl.h"
#include "util.h"
#include "fw.h"
#include "main.h"
#include "wmm.h"
#include "11n.h"

/*
 * This function resets the connection state.
 *
 * The function is invoked after receiving a disconnect event from firmware,
 * and performs the following actions -
 *      - Set media status to disconnected
 *      - Clean up Tx and Rx packets
 *      - Resets SNR/NF/RSSI value in driver
 *      - Resets security configurations in driver
 *      - Enables auto data rate
 *      - Saves the previous SSID and BSSID so that they can
 *        be used for re-association, if required
 *      - Erases current SSID and BSSID information
 *      - Sends a disconnect event to upper layers/applications.
 */
void
mwifiex_reset_connect_state(struct mwifiex_private *priv)
{
	struct mwifiex_adapter *adapter = priv->adapter;

	if (!priv->media_connected)
		return;

	dev_dbg(adapter->dev, "info: handles disconnect event\n");

	priv->media_connected = false;

	priv->scan_block = false;

	/* Free Tx and Rx packets, report disconnect to upper layer */
	mwifiex_clean_txrx(priv);

	/* Reset SNR/NF/RSSI values */
	priv->data_rssi_last = 0;
	priv->data_nf_last = 0;
	priv->data_rssi_avg = 0;
	priv->data_nf_avg = 0;
	priv->bcn_rssi_last = 0;
	priv->bcn_nf_last = 0;
	priv->bcn_rssi_avg = 0;
	priv->bcn_nf_avg = 0;
	priv->rxpd_rate = 0;
	priv->rxpd_htinfo = 0;
	priv->sec_info.wpa_enabled = false;
	priv->sec_info.wpa2_enabled = false;
	priv->wpa_ie_len = 0;

	priv->sec_info.wapi_enabled = false;
	priv->wapi_ie_len = 0;
	priv->sec_info.wapi_key_on = false;

	priv->sec_info.encryption_mode = 0;

	/* Enable auto data rate */
	priv->is_data_rate_auto = true;
	priv->data_rate = 0;

	if (priv->bss_mode == NL80211_IFTYPE_ADHOC) {
		priv->adhoc_state = ADHOC_IDLE;
		priv->adhoc_is_link_sensed = false;
	}

	/*
	 * Memorize the previous SSID and BSSID so
	 * it could be used for re-assoc
	 */

	dev_dbg(adapter->dev, "info: previous SSID=%s, SSID len=%u\n",
		priv->prev_ssid.ssid, priv->prev_ssid.ssid_len);

	dev_dbg(adapter->dev, "info: current SSID=%s, SSID len=%u\n",
		priv->curr_bss_params.bss_descriptor.ssid.ssid,
		priv->curr_bss_params.bss_descriptor.ssid.ssid_len);

	memcpy(&priv->prev_ssid,
	       &priv->curr_bss_params.bss_descriptor.ssid,
	       sizeof(struct cfg80211_ssid));

	memcpy(priv->prev_bssid,
	       priv->curr_bss_params.bss_descriptor.mac_address, ETH_ALEN);

	/* Need to erase the current SSID and BSSID info */
	memset(&priv->curr_bss_params, 0x00, sizeof(priv->curr_bss_params));

	adapter->tx_lock_flag = false;
	adapter->pps_uapsd_mode = false;

	if (adapter->num_cmd_timeout && adapter->curr_cmd)
		return;
	priv->media_connected = false;
	dev_dbg(adapter->dev,
		"info: successfully disconnected from %pM: reason code %d\n",
		priv->cfg_bssid, WLAN_REASON_DEAUTH_LEAVING);
	if (priv->bss_mode == NL80211_IFTYPE_STATION) {
		cfg80211_disconnected(priv->netdev, WLAN_REASON_DEAUTH_LEAVING,
				      NULL, 0, GFP_KERNEL);
	}
	memset(priv->cfg_bssid, 0, ETH_ALEN);

	if (!netif_queue_stopped(priv->netdev))
		mwifiex_stop_net_dev_queue(priv->netdev, adapter);
	if (netif_carrier_ok(priv->netdev))
		netif_carrier_off(priv->netdev);
	/* Reset wireless stats signal info */
	priv->qual_level = 0;
	priv->qual_noise = 0;
}

/*
 * This function handles events generated by firmware.
 *
 * This is a generic function and handles all events.
 *
 * Event specific routines are called by this function based
 * upon the generated event cause.
 *
 * For the following events, the function just forwards them to upper
 * layers, optionally recording the change -
 *      - EVENT_LINK_SENSED
 *      - EVENT_MIC_ERR_UNICAST
 *      - EVENT_MIC_ERR_MULTICAST
 *      - EVENT_PORT_RELEASE
 *      - EVENT_RSSI_LOW
 *      - EVENT_SNR_LOW
 *      - EVENT_MAX_FAIL
 *      - EVENT_RSSI_HIGH
 *      - EVENT_SNR_HIGH
 *      - EVENT_DATA_RSSI_LOW
 *      - EVENT_DATA_SNR_LOW
 *      - EVENT_DATA_RSSI_HIGH
 *      - EVENT_DATA_SNR_HIGH
 *      - EVENT_LINK_QUALITY
 *      - EVENT_PRE_BEACON_LOST
 *      - EVENT_IBSS_COALESCED
 *      - EVENT_WEP_ICV_ERR
 *      - EVENT_BW_CHANGE
 *      - EVENT_HOSTWAKE_STAIE
  *
 * For the following events, no action is taken -
 *      - EVENT_MIB_CHANGED
 *      - EVENT_INIT_DONE
 *      - EVENT_DUMMY_HOST_WAKEUP_SIGNAL
 *
 * Rest of the supported events requires driver handling -
 *      - EVENT_DEAUTHENTICATED
 *      - EVENT_DISASSOCIATED
 *      - EVENT_LINK_LOST
 *      - EVENT_PS_SLEEP
 *      - EVENT_PS_AWAKE
 *      - EVENT_DEEP_SLEEP_AWAKE
 *      - EVENT_HS_ACT_REQ
 *      - EVENT_ADHOC_BCN_LOST
 *      - EVENT_BG_SCAN_REPORT
 *      - EVENT_WMM_STATUS_CHANGE
 *      - EVENT_ADDBA
 *      - EVENT_DELBA
 *      - EVENT_BA_STREAM_TIEMOUT
 *      - EVENT_AMSDU_AGGR_CTRL
 */
int mwifiex_process_sta_event(struct mwifiex_private *priv)
{
	struct mwifiex_adapter *adapter = priv->adapter;
	int ret = 0;
	u32 eventcause = adapter->event_cause;

	switch (eventcause) {
	case EVENT_DUMMY_HOST_WAKEUP_SIGNAL:
		dev_err(adapter->dev,
			"invalid EVENT: DUMMY_HOST_WAKEUP_SIGNAL, ignore it\n");
		break;
	case EVENT_LINK_SENSED:
		dev_dbg(adapter->dev, "event: LINK_SENSED\n");
		if (!netif_carrier_ok(priv->netdev))
			netif_carrier_on(priv->netdev);
		if (netif_queue_stopped(priv->netdev))
			mwifiex_wake_up_net_dev_queue(priv->netdev, adapter);
		break;

	case EVENT_DEAUTHENTICATED:
		dev_dbg(adapter->dev, "event: Deauthenticated\n");
		adapter->dbg.num_event_deauth++;
		if (priv->media_connected)
			mwifiex_reset_connect_state(priv);
		break;

	case EVENT_DISASSOCIATED:
		dev_dbg(adapter->dev, "event: Disassociated\n");
		adapter->dbg.num_event_disassoc++;
		if (priv->media_connected)
			mwifiex_reset_connect_state(priv);
		break;

	case EVENT_LINK_LOST:
		dev_dbg(adapter->dev, "event: Link lost\n");
		adapter->dbg.num_event_link_lost++;
		if (priv->media_connected)
			mwifiex_reset_connect_state(priv);
		break;

	case EVENT_PS_SLEEP:
		dev_dbg(adapter->dev, "info: EVENT: SLEEP\n");

		adapter->ps_state = PS_STATE_PRE_SLEEP;

		mwifiex_check_ps_cond(adapter);
		break;

	case EVENT_PS_AWAKE:
		dev_dbg(adapter->dev, "info: EVENT: AWAKE\n");
		if (!adapter->pps_uapsd_mode &&
		    priv->media_connected && adapter->sleep_period.period) {
				adapter->pps_uapsd_mode = true;
				dev_dbg(adapter->dev,
					"event: PPS/UAPSD mode activated\n");
		}
		adapter->tx_lock_flag = false;
		if (adapter->pps_uapsd_mode && adapter->gen_null_pkt) {
			if (mwifiex_check_last_packet_indication(priv)) {
				if (adapter->data_sent) {
					adapter->ps_state = PS_STATE_AWAKE;
					adapter->pm_wakeup_card_req = false;
					adapter->pm_wakeup_fw_try = false;
					break;
				}
				if (!mwifiex_send_null_packet
					(priv,
					 MWIFIEX_TxPD_POWER_MGMT_NULL_PACKET |
					 MWIFIEX_TxPD_POWER_MGMT_LAST_PACKET))
						adapter->ps_state =
							PS_STATE_SLEEP;
					return 0;
			}
		}
		adapter->ps_state = PS_STATE_AWAKE;
		adapter->pm_wakeup_card_req = false;
		adapter->pm_wakeup_fw_try = false;

		break;

	case EVENT_DEEP_SLEEP_AWAKE:
		adapter->if_ops.wakeup_complete(adapter);
		dev_dbg(adapter->dev, "event: DS_AWAKE\n");
		if (adapter->is_deep_sleep)
			adapter->is_deep_sleep = false;
		break;

	case EVENT_HS_ACT_REQ:
		dev_dbg(adapter->dev, "event: HS_ACT_REQ\n");
		ret = mwifiex_send_cmd_async(priv,
					     HostCmd_CMD_802_11_HS_CFG_ENH,
					     0, 0, NULL);
		break;

	case EVENT_MIC_ERR_UNICAST:
		dev_dbg(adapter->dev, "event: UNICAST MIC ERROR\n");
		break;

	case EVENT_MIC_ERR_MULTICAST:
		dev_dbg(adapter->dev, "event: MULTICAST MIC ERROR\n");
		break;
	case EVENT_MIB_CHANGED:
	case EVENT_INIT_DONE:
		break;

	case EVENT_ADHOC_BCN_LOST:
		dev_dbg(adapter->dev, "event: ADHOC_BCN_LOST\n");
		priv->adhoc_is_link_sensed = false;
		mwifiex_clean_txrx(priv);
		if (!netif_queue_stopped(priv->netdev))
			mwifiex_stop_net_dev_queue(priv->netdev, adapter);
		if (netif_carrier_ok(priv->netdev))
			netif_carrier_off(priv->netdev);
		break;

	case EVENT_BG_SCAN_REPORT:
		dev_dbg(adapter->dev, "event: BGS_REPORT\n");
		ret = mwifiex_send_cmd_async(priv,
					     HostCmd_CMD_802_11_BG_SCAN_QUERY,
					     HostCmd_ACT_GEN_GET, 0, NULL);
		break;

	case EVENT_PORT_RELEASE:
		dev_dbg(adapter->dev, "event: PORT RELEASE\n");
		break;

	case EVENT_WMM_STATUS_CHANGE:
		dev_dbg(adapter->dev, "event: WMM status changed\n");
		ret = mwifiex_send_cmd_async(priv, HostCmd_CMD_WMM_GET_STATUS,
					     0, 0, NULL);
		break;

	case EVENT_RSSI_LOW:
		dev_dbg(adapter->dev, "event: Beacon RSSI_LOW\n");
		break;
	case EVENT_SNR_LOW:
		dev_dbg(adapter->dev, "event: Beacon SNR_LOW\n");
		break;
	case EVENT_MAX_FAIL:
		dev_dbg(adapter->dev, "event: MAX_FAIL\n");
		break;
	case EVENT_RSSI_HIGH:
		dev_dbg(adapter->dev, "event: Beacon RSSI_HIGH\n");
		break;
	case EVENT_SNR_HIGH:
		dev_dbg(adapter->dev, "event: Beacon SNR_HIGH\n");
		break;
	case EVENT_DATA_RSSI_LOW:
		dev_dbg(adapter->dev, "event: Data RSSI_LOW\n");
		break;
	case EVENT_DATA_SNR_LOW:
		dev_dbg(adapter->dev, "event: Data SNR_LOW\n");
		break;
	case EVENT_DATA_RSSI_HIGH:
		dev_dbg(adapter->dev, "event: Data RSSI_HIGH\n");
		break;
	case EVENT_DATA_SNR_HIGH:
		dev_dbg(adapter->dev, "event: Data SNR_HIGH\n");
		break;
	case EVENT_LINK_QUALITY:
		dev_dbg(adapter->dev, "event: Link Quality\n");
		break;
	case EVENT_PRE_BEACON_LOST:
		dev_dbg(adapter->dev, "event: Pre-Beacon Lost\n");
		break;
	case EVENT_IBSS_COALESCED:
		dev_dbg(adapter->dev, "event: IBSS_COALESCED\n");
		ret = mwifiex_send_cmd_async(priv,
				HostCmd_CMD_802_11_IBSS_COALESCING_STATUS,
				HostCmd_ACT_GEN_GET, 0, NULL);
		break;
	case EVENT_ADDBA:
		dev_dbg(adapter->dev, "event: ADDBA Request\n");
		mwifiex_send_cmd_async(priv, HostCmd_CMD_11N_ADDBA_RSP,
				       HostCmd_ACT_GEN_SET, 0,
				       adapter->event_body);
		break;
	case EVENT_DELBA:
		dev_dbg(adapter->dev, "event: DELBA Request\n");
		mwifiex_11n_delete_ba_stream(priv, adapter->event_body);
		break;
	case EVENT_BA_STREAM_TIEMOUT:
		dev_dbg(adapter->dev, "event:  BA Stream timeout\n");
		mwifiex_11n_ba_stream_timeout(priv,
					      (struct host_cmd_ds_11n_batimeout
					       *)
					      adapter->event_body);
		break;
	case EVENT_AMSDU_AGGR_CTRL:
		dev_dbg(adapter->dev, "event:  AMSDU_AGGR_CTRL %d\n",
			*(u16 *) adapter->event_body);
		adapter->tx_buf_size =
			min(adapter->curr_tx_buf_size,
			    le16_to_cpu(*(__le16 *) adapter->event_body));
		dev_dbg(adapter->dev, "event: tx_buf_size %d\n",
			adapter->tx_buf_size);
		break;

	case EVENT_WEP_ICV_ERR:
		dev_dbg(adapter->dev, "event: WEP ICV error\n");
		break;

	case EVENT_BW_CHANGE:
		dev_dbg(adapter->dev, "event: BW Change\n");
		break;

	case EVENT_HOSTWAKE_STAIE:
		dev_dbg(adapter->dev, "event: HOSTWAKE_STAIE %d\n", eventcause);
		break;
	default:
		dev_dbg(adapter->dev, "event: unknown event id: %#x\n",
			eventcause);
		break;
	}

	return ret;
}
