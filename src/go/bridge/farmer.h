#ifndef ARCHIVAS_FARMER_BRIDGE_H
#define ARCHIVAS_FARMER_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Farmer lifecycle
int archivas_farmer_start(char* node_url, char* plots_path, char* farmer_privkey);
void archivas_farmer_stop();
int archivas_farmer_is_running();

// Status queries
int archivas_farmer_get_plot_count();
char* archivas_farmer_get_last_proof();

// Plot creation
int archivas_farmer_create_plot(char* plot_path, unsigned int k_size, char* farmer_privkey_path);

// Logging callback
typedef void (*farmer_log_callback_t)(char* level, char* message);
void archivas_farmer_set_log_callback(farmer_log_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // ARCHIVAS_FARMER_BRIDGE_H

