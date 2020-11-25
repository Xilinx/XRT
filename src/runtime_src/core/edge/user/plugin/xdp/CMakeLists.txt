file(GLOB XRT_EDGE_USER_PLUGIN_XDP_FILES "*.cpp")
add_library(core_edgeuser_plugin_xdp_no_dl_load_objects OBJECT ${XRT_EDGE_USER_PLUGIN_XDP_FILES})
add_library(core_edgeuser_plugin_xdp_objects OBJECT ${XRT_EDGE_USER_PLUGIN_XDP_FILES})
