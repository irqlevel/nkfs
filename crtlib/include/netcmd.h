#pragma once

extern asmlinkage int net_cmd_check_sign(struct ds_net_cmd *cmd);
extern asmlinkage int net_cmd_check_unique(struct ds_net_cmd *cmd, u64 unique);
extern asmlinkage void net_cmd_init(struct ds_net_cmd *cmd);
