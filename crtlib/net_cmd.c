#include <crtlib/include/crtlib.h>

int net_cmd_check_sign(struct ds_net_cmd *cmd)
{
	return (cmd->sign == DS_NET_CMD_SIGN) ? 0 : -DS_E_INVAL;
}
EXPORT_SYMBOL(net_cmd_check_sign);

int net_cmd_check_unique(struct ds_net_cmd *cmd, u64 unique)
{
	return (cmd->unique == unique) ? 0 : -DS_E_INVAL;
}
EXPORT_SYMBOL(net_cmd_check_unique);

void net_cmd_init(struct ds_net_cmd *cmd)
{
	crt_memset(cmd, 0, sizeof(*cmd));
	cmd->sign = DS_NET_CMD_SIGN;
	cmd->unique = rand_u64();
}
EXPORT_SYMBOL(net_cmd_init);
