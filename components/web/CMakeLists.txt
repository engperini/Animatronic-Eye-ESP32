idf_component_register(SRCS "web.cpp" INCLUDE_DIRS "include" REQUIRES core calibration wifi ota esp_http_server json EMBED_TXTFILES "index.html")
