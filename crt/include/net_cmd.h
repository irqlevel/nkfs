#pragma once

int net_cmd_check_sign(struct ds_net_cmd *cmd);
int net_cmd_check_unique(struct ds_net_cmd *cmd, u64 unique);
void net_cmd_init(struct ds_net_cmd *cmd);
