/*
** Zabbix
** Copyright (C) 2001-2018 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "common.h"
#include "log.h"
#include "zbxserialize.h"
#include "zbxipcservice.h"
#include "lld_protocol.h"

/******************************************************************************
 *                                                                            *
 * Function: zbx_lld_serialize_item_value                                     *
 *                                                            goto cleanup;                *
 ******************************************************************************/
zbx_uint32_t	zbx_lld_serialize_item_value(unsigned char **data, zbx_uint64_t itemid, const char *value,
		const zbx_timespec_t *ts, const char *error)
{
	unsigned char	*ptr;
	zbx_uint32_t	data_len = 0, value_len, error_len;

	zbx_serialize_prepare_value(data_len, itemid);
	zbx_serialize_prepare_str(data_len, value);
	zbx_serialize_prepare_value(data_len, zbx_timespec_t);
	zbx_serialize_prepare_str(data_len, error);

	*data = (unsigned char *)zbx_malloc(NULL, data_len);

	ptr = *data;
	ptr += zbx_serialize_value(ptr, itemid);
	ptr += zbx_serialize_str(ptr, value, value_len);
	ptr += zbx_serialize_value(ptr, *ts);
	(void)zbx_serialize_str(ptr, error, error_len);

	return data_len;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_lld_deserialize_item_value                                   *
 *                                                                            *
 ******************************************************************************/
void	zbx_lld_deserialize_item_value(const unsigned char *data, zbx_uint64_t *itemid, char **value,
		zbx_timespec_t *ts, char **error)
{
	zbx_uint32_t	value_len, error_len;

	data += zbx_deserialize_value(data, itemid);
	data += zbx_deserialize_str(data, value, value_len);
	data += zbx_deserialize_value(data, ts);
	(void)zbx_deserialize_str(data, error, error_len);
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_lld_process_value                                            *
 *                                                                            *
 * Purpose: process low level discovery value/error                           *
 *                                                                            *
 * Parameters: itemid - [IN] the LLD rule id                                  *
 *             value  - [IN] the rule value (can be NULL if error is set)     *
 *             ts     - [IN] the value timestamp                              *
 *             error  - [IN] the error message (can be NULL)                  *
 *                                                                            *
 ******************************************************************************/
void	zbx_lld_process_value(zbx_uint64_t itemid, const char *value, const zbx_timespec_t *ts, const char *error)
{
	static zbx_ipc_socket_t	socket = {0};
	char			*errmsg;
	unsigned char		*data;
	zbx_uint32_t		data_len;

	/* each process has a permanent connection to preprocessing manager */
	if (0 == socket.fd && FAIL == zbx_ipc_socket_open(&socket, ZBX_IPC_SERVICE_LLD, SEC_PER_MIN, &errmsg))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot connect to LLD manager service: %s", errmsg);
		exit(EXIT_FAILURE);
	}

	data_len = zbx_lld_serialize_item_value(&data, itemid, value, ts, error);

	if (FAIL == zbx_ipc_socket_write(&socket, ZBX_IPC_LLD_REQUEST, data, data_len))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot send data to LLD manager service");
		exit(EXIT_FAILURE);
	}

	zbx_free(data);
}


/******************************************************************************
 *                                                                            *
 * Function: zbx_lld_get_queue_size                                           *
 *                                                                            *
 * Purpose: get queue size (enqueued value count) of LLD manager              *
 *                                                                            *
 * Parameters: size  - [OUT] the queue size                                   *
 *             error - [OUT] the error message                                *
 *                                                                            *
 * Return value: SUCCEED - the queue size was returned successfully           *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 ******************************************************************************/
int	zbx_lld_get_queue_size(zbx_uint64_t *size, char **error)
{
	zbx_ipc_message_t	message;
	zbx_ipc_socket_t	lld_socket;
	int			ret = FAIL;

	if (FAIL == zbx_ipc_socket_open(&lld_socket, ZBX_IPC_SERVICE_LLD, SEC_PER_MIN, error))
		return FAIL;

	zbx_ipc_message_init(&message);

	if (FAIL == zbx_ipc_socket_write(&lld_socket, ZBX_IPC_LLD_QUEUE, NULL, 0))
	{
		*error = zbx_strdup(NULL, "cannot send queue request to LLD manager service");
		goto out;
	}

	if (FAIL == zbx_ipc_socket_read(&lld_socket, &message))
	{
		*error = zbx_strdup(NULL, "cannot read queue response from LLD manager service");
		goto out;
	}

	memcpy(size, message.data, sizeof(zbx_uint64_t));
	ret = SUCCEED;
out:
	zbx_ipc_socket_close(&lld_socket);
	zbx_ipc_message_clean(&message);

	return ret;
}
