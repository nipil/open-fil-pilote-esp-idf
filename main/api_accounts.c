#include <cjson.h>
#include <esp_log.h>

#include "str.h"
#include "ofp.h"
#include "webserver.h"
#include "api_accounts.h"

static const char TAG[] = "api_accounts";

/***************************************************************************/

esp_err_t serve_api_get_accounts(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_accounts version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    struct ofp_account **account_list = ofp_account_list_get();
    if (account_list == NULL)
        goto cleanup;

    cJSON *root = cJSON_CreateObject();
    cJSON *accounts = cJSON_AddArrayToObject(root, json_key_accounts);

    struct ofp_session_context *o = req->sess_ctx;
    for (int i = 0; i < OFP_MAX_ACCOUNT_COUNT; i++)
    {
        struct ofp_account *account = account_list[i];
        if (account == NULL)
            continue;

        // restrict access to allowed data
        if (o == NULL || (!o->user_is_admin && (strcmp(o->user_id, account->id) != 0)))
            continue;

        cJSON *jacc = cJSON_CreateObject();
        cJSON_AddItemToArray(accounts, jacc);
        cJSON_AddStringToObject(jacc, json_key_id, account->id);
    }

    esp_err_t result = serve_json(req, root);
    cJSON_Delete(root);
    return result;

cleanup:
    return httpd_resp_send_500(req);
}

esp_err_t serve_api_post_accounts(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_post_accounts version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // TODO: not yet implemented

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_delete_accounts_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    char *id = re_get_string(captures, 2);
    ESP_LOGD(TAG, "serve_api_delete_accounts_id version=%i id=%s", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // restrict access to allowed data
    if (!ofp_session_user_is_admin_or_self(req, id))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Unauthorized");

    // prevent deleting admin account
    if (strcmp(id, admin_str) == 0)
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot remove admin account");

    // remove account
    if (!ofp_account_list_remove_existing_account(id))
    {
        ESP_LOGW(TAG, "Could not remove account %s", id);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not remove account");
    }

    ESP_LOGW(TAG, "Account '%s' removed", id);
    return httpd_resp_sendstr(req, "Account deleted");
}

esp_err_t serve_api_patch_accounts_id(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    char *id = re_get_string(captures, 2);
    ESP_LOGD(TAG, "serve_api_patch_accounts_id version=%i id=%s", version, id);
    if (version != 1)
        return httpd_resp_send_404(req);

    // restrict access to allowed data
    if (!ofp_session_user_is_admin_or_self(req, id))
        return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Unauthorized");

    // read
    char *buf = webserver_get_request_data_atomic(req);
    if (buf == NULL)
    {
        ESP_LOGW(TAG, "Failed getting request data");
        return ESP_FAIL;
    }

    // parse
    cJSON *root = cJSON_Parse(buf);

    // cleanup
    free(buf);

    // parse error
    if (root == NULL)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed parsing JSON body");

    // password is optional
    char *new_cleartext = NULL;
    if (cjson_get_child_string(root, json_key_password, &new_cleartext) == JSON_HELPER_RESULT_INVALID)
    {
        cJSON_Delete(root);
        ESP_LOGD(TAG, "Invalid password");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid password");
    }
    if (new_cleartext != NULL && !ofp_account_list_reset_password_account(id, new_cleartext))
    {
        cJSON_Delete(root);
        ESP_LOGW(TAG, "Could not change password for account %s", id);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not change password");
    }

    // not providing any matching element is not an error

    // cleanup
    cJSON_Delete(root);

    ESP_LOGW(TAG, "Password for account '%s' was changed", id);
    return httpd_resp_sendstr(req, "Password changed");
}