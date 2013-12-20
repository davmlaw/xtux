void cl_network_init(void);
int cl_network_connect(char *sv_name, int port);
void cl_net_connection_lost(char *str);
void cl_network_disconnect(void);
int cl_net_handle_message(netmsg msg);
int cl_net_update(void);
void cl_net_finish(void);
void cl_net_stats(void);
