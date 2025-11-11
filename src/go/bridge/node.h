#ifndef ARCHIVAS_NODE_BRIDGE_H
#define ARCHIVAS_NODE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Node lifecycle
int archivas_node_start(char* network_id, char* rpc_bind, char* data_dir, char* bootnodes, char* genesis_path);
void archivas_node_stop();
int archivas_node_is_running();

// Status queries
int archivas_node_get_height();
char* archivas_node_get_tip_hash();
int archivas_node_get_peer_count();

// Logging callback
typedef void (*log_callback_t)(char* level, char* message);
void archivas_node_set_log_callback(log_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // ARCHIVAS_NODE_BRIDGE_H

