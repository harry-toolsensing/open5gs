/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "sbi-path.h"
#include "binding.h"
#include "namf-handler.h"

bool smf_namf_comm_handler_n1_n2_message_transfer(
        smf_sess_t *sess, int state, ogs_sbi_message_t *recvmsg)
{
    smf_ue_t *smf_ue = NULL;

    ogs_assert(sess);
    smf_ue = sess->smf_ue;
    ogs_assert(smf_ue);
    ogs_assert(state);
    ogs_assert(recvmsg);

    switch (state) {
    case SMF_UE_REQUESTED_PDU_SESSION_ESTABLISHMENT:
        if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_OK) {
            smf_qos_flow_binding(sess, NULL);
        } else {
            ogs_error("[%s:%d] HTTP response error [%d]",
                smf_ue->supi, sess->psi, recvmsg->res_status);
        }
        break;

    case SMF_NETWORK_REQUESTED_QOS_FLOW_MODIFICATION:
        if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_OK) {
            /* Nothing */
        } else {
            ogs_error("[%s:%d] HTTP response error [%d]",
                smf_ue->supi, sess->psi, recvmsg->res_status);
        }
        break;

    case SMF_NETWORK_TRIGGERED_SERVICE_REQUEST:
        if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_OK) {
            /* Nothing */
        } else if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_ACCEPTED) {
            if (recvmsg->http.location)
                smf_sess_set_paging_n1n2message_location(
                        sess, recvmsg->http.location);
            else
                ogs_error("No HTTP Location");
        } else {
            ogs_error("[%s:%d] HTTP response error [%d]",
                smf_ue->supi, sess->psi, recvmsg->res_status);
        }
        break;

    default:
        ogs_fatal("Unexpected state [%d]", state);
        ogs_assert_if_reached();
    }

    return true;
}

bool smf_namf_comm_handler_n1_n2_message_transfer_failure_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    OpenAPI_n1_n2_msg_txfr_failure_notification_t
        *N1N2MsgTxfrFailureNotification = NULL;

    smf_sess_t *sess = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    N1N2MsgTxfrFailureNotification = recvmsg->N1N2MsgTxfrFailureNotification;
    if (!N1N2MsgTxfrFailureNotification) {
        ogs_error("No N1N2MsgTxfrFailureNotification");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "No N1N2MsgTxfrFailureNotification", NULL);
        return false;
    }

    if (!N1N2MsgTxfrFailureNotification->cause) {
        ogs_error("No Cause");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "No Cause", NULL);
        return false;
    }

    if (!N1N2MsgTxfrFailureNotification->n1n2_msg_data_uri) {
        ogs_error("No n1n2MsgDataUri");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "No n1n2MsgDataUri", NULL);
        return false;
    }

    sess = smf_sess_find_by_paging_n1n2message_location(
        N1N2MsgTxfrFailureNotification->n1n2_msg_data_uri);
    if (!sess) {
        ogs_error("Not found");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
            recvmsg, N1N2MsgTxfrFailureNotification->n1n2_msg_data_uri, NULL);
        return false;
    }

    smf_sbi_send_http_status_no_content(stream);
    return true;
}
