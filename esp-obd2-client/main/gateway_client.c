#include "gateway_client.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include "esp_log.h"
#include "lwip/inet.h"
#include "sdkconfig.h"

static const char *TAG = "gateway";
static int s_socket = -1;

void gateway_client_close(void)
{
    if (s_socket >= 0) {
        shutdown(s_socket, SHUT_RDWR);
        close(s_socket);
        s_socket = -1;
    }
}

static esp_err_t connect_gateway(void)
{
    gateway_client_close();
    s_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (s_socket < 0) return ESP_FAIL;
    struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
    setsockopt(s_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(s_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_OBD_GATEWAY_PORT)
    };
    if (inet_pton(AF_INET, CONFIG_OBD_GATEWAY_HOST, &address.sin_addr) != 1) {
        gateway_client_close();
        return ESP_ERR_INVALID_ARG;
    }
    if (connect(s_socket, (struct sockaddr *)&address, sizeof(address)) != 0) {
        ESP_LOGW(TAG, "connect failed: errno=%d", errno);
        gateway_client_close();
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t send_all(const char *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        const int count = send(s_socket, data + sent, length - sent, 0);
        if (count <= 0) return ESP_FAIL;
        sent += (size_t)count;
    }
    return ESP_OK;
}

esp_err_t gateway_client_send(const char *json)
{
    if (json == NULL) return ESP_ERR_INVALID_ARG;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (s_socket < 0 && connect_gateway() != ESP_OK) continue;
        if (send_all(json, strlen(json)) != ESP_OK ||
            send_all("\n", 1) != ESP_OK) {
            gateway_client_close();
            continue;
        }
        char response[64] = {0};
        const int received = recv(s_socket, response, sizeof(response) - 1, 0);
        if (received > 0) {
            response[received] = '\0';
            if (strstr(response, "OK") != NULL) {
                ESP_LOGI(TAG, "Gateway ACK received");
                gateway_client_close();
                return ESP_OK;
            }
            ESP_LOGW(TAG, "invalid ACK (%d bytes): %s", received, response);
        } else {
            ESP_LOGW(TAG, "ACK recv failed: received=%d errno=%d", received, errno);
        }
        gateway_client_close();
    }
    return ESP_FAIL;
}
