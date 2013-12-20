void sv_netmsg_send_message(client_t *cl, byte message_type);
void sv_netmsg_send_rejection(netconnection_t *nc, char *reason);

void sv_netmsg_send_version(client_t *cl);
void sv_netmsg_send_textmessage(client_t *cl, char *sender, char *string);
void sv_netmsg_send_sv_info(client_t *cl);
void sv_netmsg_send_changelevel(client_t *cl);
void sv_netmsg_send_update_statusbar(client_t *cl);
void sv_netmsg_send_gamemessage(client_t *cl, char *str, int everyone);
void sv_netmsg_send_map_target(client_t *cl, int active, shortpoint_t target);
void sv_netmsg_send_objective(client_t *cl, char *str, int everyone);
void sv_netmsg_send_frags(client_t *cl, int everyone);

