short sv_net_init(short port);
void sv_net_get_client_input(void);
void sv_net_update_clients(void);
void sv_net_send_start_frames(void);
void sv_net_send_to_all(netmsg msg);
void sv_net_send_text_to_all(char *message);
void sv_net_tell_clients_to_changelevel(void);
void sv_net_remove_client(client_t *cl);
