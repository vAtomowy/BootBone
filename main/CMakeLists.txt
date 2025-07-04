# AB

set (src    "./main.c"
            "nvs_store.c"
            "webserver.c"
    )

set (inc    "."
            "./include/"
    )

idf_component_register(SRCS ${src}
                    PRIV_REQUIRES spi_flash
                    REQUIRES driver nvs_flash esp_wifi esp_event esp_http_server esp_netif spiffs
                    INCLUDE_DIRS ${inc})