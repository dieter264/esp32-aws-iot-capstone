set(CERT_DIR "${CMAKE_CURRENT_LIST_DIR}/certs")
set(CRT_FILE "${CERT_DIR}/server.crt")
set(KEY_FILE "${CERT_DIR}/server.key")

if(NOT EXISTS "${CRT_FILE}" OR NOT EXISTS "${KEY_FILE}")
    message(
        FATAL_ERROR
        "!!! HTTPS cert/key missing !!!"
        "  ${CRT_FILE}"
        "  ${KEY_FILE}"
        "See README for openssl commands."
    )
endif()

set(WEB_DIR "${CMAKE_CURRENT_LIST_DIR}/web")
set(CONFIG_HTML_FILE "${WEB_DIR}/config.html")

if(NOT EXISTS "${CONFIG_HTML_FILE}")
    message(FATAL_ERROR "!!! HTML forms missing !!!" "  ${CONFIG_HTML_FILE}")
endif()

idf_component_register(
    SRCS
        "https_server.cpp"
        "https_srv_basic_auth.cpp"
        "https_srv_uri_handler.cpp"
        "https_srv_url_decoder.cpp"
    INCLUDE_DIRS 
        "include"
    PRIV_INCLUDE_DIRS 
        "private_include"
    PRIV_REQUIRES
        esp_https_server
        app_cmn
        app_config_store
        app_state_machine
    EMBED_TXTFILES
        "certs/server.crt"
        "certs/server.key"
        "web/config.html"
)
