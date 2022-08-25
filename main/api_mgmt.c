#include <cjson.h>
#include <esp_log.h>

#include "ofp.h"
#include "webserver.h"
#include "api_mgmt.h"

static const char TAG[] = "api_mgmt";

#define REBOOT_WAIT_SEC 10

/***************************************************************************/

static void reboot_wait(void *pvParameters)
{
    ESP_LOGI(TAG, "Rebooting in %i seconds...", REBOOT_WAIT_SEC);
    wait_sec(REBOOT_WAIT_SEC);

    ESP_LOGI(TAG, "Rebooting NOW !");
    // Task should NOT exit (or BY DEFAULT it causes FreeRTOS to abort()
    esp_restart();
}

static void mgmt_queue_reboot(void)
{
    // notify the webserver to not serve anything anymore
    webserver_disable();

    // start task for delayed reboot
    TaskHandle_t xHandle = NULL;
    xTaskCreatePinnedToCore(reboot_wait, "reboot_wait", 2048, NULL, 1, &xHandle, 1);
    configASSERT(xHandle);
}

/***************************************************************************/

esp_err_t serve_api_get_status(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_status version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    return httpd_resp_send_500(req);
}

esp_err_t serve_api_get_reboot(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_get_reboot version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    // dump some dev stats before rebooting
    uint32_t min = esp_get_minimum_free_heap_size();
    ESP_LOGD(TAG, "Minimum heap that has ever been available: %u", min);

    // prepare restart, giving it the time to serve the page and cleanup
    mgmt_queue_reboot();

    // serve wait page
    return serve_static_ofp_wait_html(req);
}

esp_err_t serve_api_post_upgrade(httpd_req_t *req, struct re_result *captures)
{
    int version = re_get_int(captures, 1);
    ESP_LOGD(TAG, "serve_api_post_upgrade version=%i", version);
    if (version != 1)
        return httpd_resp_send_404(req);

    /*
    sample file upload from Chrome, same with Edge
    
        Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryQ63nar1W96PdzXtt
        D (72538) api_hw: content length 2499
        D (72568) api_hw: 0x3ffd0bd0   2d 2d 2d 2d 2d 2d 57 65  62 4b 69 74 46 6f 72 6d  |------WebKitForm|
        D (72578) api_hw: 0x3ffd0be0   42 6f 75 6e 64 61 72 79  51 36 33 6e 61 72 31 57  |BoundaryQ63nar1W|
        D (72588) api_hw: 0x3ffd0bf0   39 36 50 64 7a 58 74 74  0d 0a 43 6f 6e 74 65 6e  |96PdzXtt..Conten|
        D (72598) api_hw: 0x3ffd0c00   74 2d 44 69 73 70 6f 73  69 74 69 6f 6e 3a 20 66  |t-Disposition: f|
        D (72638) api_hw: 0x3ffd0bd0   6f 72 6d 2d 64 61 74 61  3b 20 6e 61 6d 65 3d 22  |orm-data; name="|
        D (72648) api_hw: 0x3ffd0be0   66 69 6c 65 22 3b 20 66  69 6c 65 6e 61 6d 65 3d  |file"; filename=|
        D (72658) api_hw: 0x3ffd0bf0   22 44 65 66 61 75 6c 74  2e 72 64 70 22 0d 0a 43  |"Default.rdp"..C|
        D (72668) api_hw: 0x3ffd0c00   6f 6e 74 65 6e 74 2d 54  79 70 65 3a 20 61 70 70  |ontent-Type: app|
        D (72718) api_hw: 0x3ffd0bd0   6c 69 63 61 74 69 6f 6e  2f 6f 63 74 65 74 2d 73  |lication/octet-s|
        D (72718) api_hw: 0x3ffd0be0   74 72 65 61 6d 0d 0a 0d  0a ff fe 73 00 63 00 72  |tream......s.c.r|
        D (72728) api_hw: 0x3ffd0bf0   00 65 00 65 00 6e 00 20  00 6d 00 6f 00 64 00 65  |.e.e.n. .m.o.d.e|
        D (72738) api_hw: 0x3ffd0c00   00 20 00 69 00 64 00 3a  00 69 00 3a 00 32 00 0d  |. .i.d.:.i.:.2..|
        ...
        D (75288) api_hw: 0x3ffd0bd0   00 79 00 6e 00 61 00 6d  00 65 00 3a 00 73 00 3a  |.y.n.a.m.e.:.s.:|
        D (75298) api_hw: 0x3ffd0be0   00 0d 00 0a 00 0d 0a 2d  2d 2d 2d 2d 2d 57 65 62  |.......------Web|
        D (75308) api_hw: 0x3ffd0bf0   4b 69 74 46 6f 72 6d 42  6f 75 6e 64 61 72 79 51  |KitFormBoundaryQ|
        D (75318) api_hw: 0x3ffd0c00   36 33 6e 61 72 31 57 39  36 50 64 7a 58 74 74 2d  |63nar1W96PdzXtt-|
        D (75358) api_hw: 0x3ffd0bd0   2d 0d 0a                                          |-..|

    sample data upload with curl

        Content-Type: application/x-www-form-urlencoded
        D (924878) api_hw: content length 30
        D (924918) api_hw: 0x3ffd0bd0   74 69 74 69 20 6c 65 20  70 69 61 66 20 65 73 74  |titi le piaf est|
        D (924928) api_hw: 0x3ffd0be0   20 6a 61 75 6e 65 65 74  20 62 c3 aa 74 65        | jauneet b..te|
    */

    return httpd_resp_send_500(req);
}