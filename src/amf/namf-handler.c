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

#include "namf-handler.h"
#include "nsmf-handler.h"

#include "nas-path.h"
#include "ngap-path.h"
#include "sbi-path.h"

int amf_namf_comm_handle_n1_n2_message_transfer(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    int status;
    OpenAPI_n1_n2_message_transfer_cause_e cause;

    amf_ue_t *amf_ue = NULL;
    amf_sess_t *sess = NULL;

    ogs_pkbuf_t *n1buf = NULL;
    ogs_pkbuf_t *n2buf = NULL;

    ogs_pkbuf_t *gmmbuf = NULL;
    ogs_pkbuf_t *ngapbuf = NULL;

    char *supi = NULL;
    uint8_t pdu_session_id = OGS_NAS_PDU_SESSION_IDENTITY_UNASSIGNED;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    OpenAPI_n1_n2_message_transfer_req_data_t *N1N2MessageTransferReqData;
    OpenAPI_n1_n2_message_transfer_rsp_data_t N1N2MessageTransferRspData;
    OpenAPI_n1_message_container_t *n1MessageContainer = NULL;
    OpenAPI_ref_to_binary_data_t *n1MessageContent = NULL;
    OpenAPI_n2_info_container_t *n2InfoContainer = NULL;
    OpenAPI_n2_sm_information_t *smInfo = NULL;
    OpenAPI_n2_info_content_t *n2InfoContent = NULL;
    OpenAPI_ref_to_binary_data_t *ngapData = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    N1N2MessageTransferReqData = recvmsg->N1N2MessageTransferReqData;
    if (!N1N2MessageTransferReqData) {
        ogs_error("No N1N2MessageTransferReqData");
        return OGS_ERROR;
    }

    pdu_session_id = N1N2MessageTransferReqData->pdu_session_id;
    if (pdu_session_id == OGS_NAS_PDU_SESSION_IDENTITY_UNASSIGNED) {
        ogs_error("No PDU Session Identity");
        return OGS_ERROR;
    }

    supi = recvmsg->h.resource.component[1];
    if (!supi) {
        ogs_error("No SUPI");
        return OGS_ERROR;
    }

    amf_ue = amf_ue_find_by_supi(supi);
    if (!amf_ue) {
        ogs_error("No UE context [%s]", supi);
        return OGS_ERROR;
    }

    sess = amf_sess_find_by_psi(amf_ue, pdu_session_id);
    if (!sess) {
        ogs_error("[%s] No PDU Session Context [%d]",
                amf_ue->supi, pdu_session_id);
        return OGS_ERROR;
    }

    n1MessageContainer = N1N2MessageTransferReqData->n1_message_container;
    if (n1MessageContainer) {
        n1MessageContent = n1MessageContainer->n1_message_content;
        if (!n1MessageContent || !n1MessageContent->content_id) {
            ogs_error("No n1MessageContent");
            return OGS_ERROR;
        }

        n1buf = ogs_sbi_find_part_by_content_id(
                recvmsg, n1MessageContent->content_id);
        if (!n1buf) {
            ogs_error("[%s] No N1 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        /*
         * NOTE : The pkbuf created in the SBI message will be removed
         *        from ogs_sbi_message_free(), so it must be copied.
         */
        n1buf = ogs_pkbuf_copy(n1buf);
        ogs_assert(n1buf);
    }

    n2InfoContainer = N1N2MessageTransferReqData->n2_info_container;
    if (n2InfoContainer) {
        smInfo = n2InfoContainer->sm_info;
        if (!smInfo) {
            ogs_error("No smInfo");
            return OGS_ERROR;
        }
        n2InfoContent = smInfo->n2_info_content;
        if (!n2InfoContent) {
            ogs_error("No n2InfoContent");
            return OGS_ERROR;
        }

        ngapData = n2InfoContent->ngap_data;
        if (!ngapData || !ngapData->content_id) {
            ogs_error("No ngapData");
            return OGS_ERROR;
        }
        n2buf = ogs_sbi_find_part_by_content_id(
                recvmsg, ngapData->content_id);
        if (!n2buf) {
            ogs_error("[%s] No N2 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        /*
         * NOTE : The pkbuf created in the SBI message will be removed
         *        from ogs_sbi_message_free(), so it must be copied.
         */
        n2buf = ogs_pkbuf_copy(n2buf);
        ogs_assert(n2buf);
    }

    switch (n2InfoContent->ngap_ie_type) {
    case OpenAPI_ngap_ie_type_PDU_RES_SETUP_REQ:
    case OpenAPI_ngap_ie_type_PDU_RES_MOD_REQ:
        /* N1 SM Message */
        if (n1buf) {
            gmmbuf = gmm_build_dl_nas_transport(sess,
                    OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, n1buf, 0, 0);
            ogs_assert(gmmbuf);
        }
        break;
    default:
        ogs_error("Not implemented ngap_ie_type[%d]",
                n2InfoContent->ngap_ie_type);
        return OGS_ERROR;
    }

    status = OGS_SBI_HTTP_STATUS_OK;
    cause = OpenAPI_n1_n2_message_transfer_cause_N1_N2_TRANSFER_INITIATED;

    switch (n2InfoContent->ngap_ie_type) {
    case OpenAPI_ngap_ie_type_PDU_RES_SETUP_REQ:
        if (!n2buf) {
            ogs_error("[%s] No N2 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        if (gmmbuf) {
            ran_ue_t *ran_ue = NULL;

            /***********************************
             * 4.3.2 PDU Session Establishment *
             ***********************************/

            ran_ue = ran_ue_cycle(amf_ue->ran_ue);
            ogs_assert(ran_ue);

            if (sess->pdu_session_establishment_accept) {
                ogs_pkbuf_free(sess->pdu_session_establishment_accept);
                sess->pdu_session_establishment_accept = NULL;
            }

            if (ran_ue->initial_context_setup_request_sent == true) {
                ngapbuf = ngap_sess_build_pdu_session_resource_setup_request(
                        sess, gmmbuf, n2buf);
                ogs_assert(ngapbuf);
            } else {
                ngapbuf = ngap_sess_build_initial_context_setup_request(
                        sess, gmmbuf, n2buf);
                ogs_assert(ngapbuf);

                ran_ue->initial_context_setup_request_sent = true;
            }

            if (SESSION_CONTEXT_IN_SMF(sess)) {
                /*
                 * [1-CLIENT] /nsmf-pdusession/v1/sm-contexts
                 * [2-SERVER] /namf-comm/v1/ue-contexts/{supi}/n1-n2-messages
                 *
                 * If [2-SERVER] arrives after [1-CLIENT],
                 * sm-context-ref is created in [1-CLIENT].
                 * So, the PDU session establishment accpet can be transmitted.
                 */
                if (nas_5gs_send_to_gnb(amf_ue, ngapbuf) != OGS_OK) {
                    ogs_error("nas_5gs_send_to_gnb() failed");
                    status = OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR;
                }
            } else {
                sess->pdu_session_establishment_accept = ngapbuf;
            }

        } else {
            /*********************************************
             * 4.2.3.3 Network Triggered Service Request *
             *********************************************/

            /*
             * To Deliver N2 SM Content to gNB Temporarily,
             * Store N2 SM Context in Session Context
             */
            if (sess->transfer.pdu_session_resource_setup_request) {
                /*
                 * It should not be reached this way.
                 * If the problem occurred, free the old n2buf
                 */
                ogs_error("[%s:%d] N2 SM Content is duplicated",
                        amf_ue->supi, sess->psi);
                ogs_pkbuf_free(
                        sess->transfer.pdu_session_resource_setup_request);
            }
            /*
             * NOTE : The pkbuf created in the SBI message will be removed
             *        from ogs_sbi_message_free().
             *        So it must be copied and push a event queue.
             */
            sess->transfer.pdu_session_resource_setup_request = n2buf;
            ogs_assert(sess->transfer.pdu_session_resource_setup_request);

            if (CM_IDLE(amf_ue)) {

#if 0
                ngap_send_paging(amf_ue);
#endif

                ogs_pkbuf_free(sess->
                    transfer.pdu_session_resource_setup_request);
                sess->transfer.pdu_session_resource_setup_request = NULL;

            } else if (CM_CONNECTED(amf_ue)) {

#if 0
                ngap_send_n2_only_request(amf_ue);
#endif

                ogs_pkbuf_free(sess->
                    transfer.pdu_session_resource_setup_request);
                sess->transfer.pdu_session_resource_setup_request = NULL;

            } else {

                ogs_fatal("[%s] Invalid AMF-UE state", amf_ue->supi);
                ogs_assert_if_reached();

            }

        }
        break;

    case OpenAPI_ngap_ie_type_PDU_RES_MOD_REQ:
        if (!gmmbuf) {
            ogs_error("[%s] No N1 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }
        if (!n2buf) {
            ogs_error("[%s] No N2 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        ngapbuf = ngap_build_pdu_session_resource_modify_request(
                sess, gmmbuf, n2buf);
        ogs_assert(ngapbuf);

        if (nas_5gs_send_to_gnb(amf_ue, ngapbuf) != OGS_OK) {
            ogs_error("nas_5gs_send_to_gnb() failed");
            status = OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR;
        }
        break;

    default:
        ogs_error("Not implemented ngap_ie_type[%d]",
                n2InfoContent->ngap_ie_type);
        ogs_assert_if_reached();
    }

    memset(&sendmsg, 0, sizeof(sendmsg));

    if (status == OGS_SBI_HTTP_STATUS_OK) {
        memset(&N1N2MessageTransferRspData, 0,
                sizeof(N1N2MessageTransferRspData));
        N1N2MessageTransferRspData.cause = cause;

        sendmsg.N1N2MessageTransferRspData = &N1N2MessageTransferRspData;
    }

    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_sbi_server_send_response(stream, response);

    return OGS_OK;
}

int amf_namf_callback_handle_sm_context_status(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    int status = OGS_SBI_HTTP_STATUS_NO_CONTENT;

    amf_ue_t *amf_ue = NULL;
    amf_sess_t *sess = NULL;

    uint8_t pdu_session_identity;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    OpenAPI_sm_context_status_notification_t *SmContextStatusNotification;
    OpenAPI_status_info_t *StatusInfo;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    if (!recvmsg->h.resource.component[0]) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("No SUPI");
        goto cleanup;
    }

    amf_ue = amf_ue_find_by_supi(recvmsg->h.resource.component[0]);
    if (!amf_ue) {
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        ogs_error("Cannot find SUPI [%s]", recvmsg->h.resource.component[0]);
        goto cleanup;
    }

    if (!recvmsg->h.resource.component[2]) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s] No PDU Session Identity", amf_ue->supi);
        goto cleanup;
    }

    pdu_session_identity = atoi(recvmsg->h.resource.component[2]);
    if (pdu_session_identity == OGS_NAS_PDU_SESSION_IDENTITY_UNASSIGNED) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s] PDU Session Identity is unassigned", amf_ue->supi);
        goto cleanup;
    }

    sess = amf_sess_find_by_psi(amf_ue, pdu_session_identity);
    if (!sess) {
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        ogs_warn("[%s] Cannot find session", amf_ue->supi);
        goto cleanup;
    }

    SmContextStatusNotification = recvmsg->SmContextStatusNotification;
    if (!SmContextStatusNotification) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s:%d] No SmContextStatusNotification",
                amf_ue->supi, sess->psi);
        goto cleanup;
    }

    StatusInfo = SmContextStatusNotification->status_info;
    if (!StatusInfo) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s:%d] No StatusInfo", amf_ue->supi, sess->psi);
        goto cleanup;
    }

    sess->resource_status = StatusInfo->resource_status;

    /*
     * Race condition for PDU session release complete
     *  - CLIENT : /nsmf-pdusession/v1/sm-contexts/{smContextRef}/modify
     *  - SERVER : /namf-callback/v1/{supi}/sm-context-status/{psi})
     *
     * If NOTIFICATION is received before the CLIENT response is received,
     * CLIENT sync is not finished. In this case, the session context
     * should not be removed.
     *
     * If NOTIFICATION comes after the CLIENT response is received,
     * sync is done. So, the session context can be removed.
     */
    if (sess->n1_released == true &&
        sess->n2_released == true &&
        sess->resource_status == OpenAPI_resource_status_RELEASED) {

        ogs_debug("[%s:%d] SM context remove", amf_ue->supi, sess->psi);
        amf_nsmf_pdu_session_handle_release_sm_context(
                sess, AMF_RELEASE_SM_CONTEXT_NO_STATE);
    }

cleanup:
    memset(&sendmsg, 0, sizeof(sendmsg));

    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_sbi_server_send_response(stream, response);

    return OGS_OK;
}
