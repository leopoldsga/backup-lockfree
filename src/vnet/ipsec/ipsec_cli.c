/*
 * decap.c : IPSec tunnel support
 *
 * Copyright (c) 2015 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vnet/vnet.h>
#include <vnet/api_errno.h>
#include <vnet/ip/ip.h>
#include <vnet/interface.h>
#include <vnet/fib/fib.h>

#include <vnet/ipsec/ipsec.h>
#include <vnet/ipsec/ipsec_tun.h>

static clib_error_t *
set_interface_spd_command_fn (vlib_main_t * vm,
			      unformat_input_t * input,
			      vlib_cli_command_t * cmd)
{
  unformat_input_t _line_input, *line_input = &_line_input;
  ipsec_main_t *im = &ipsec_main;
  u32 sw_if_index = (u32) ~ 0;
  u32 spd_id;
  int is_add = 1;
  clib_error_t *error = NULL;

  if (!unformat_user (input, unformat_line_input, line_input))
    return 0;

  if (unformat
      (line_input, "%U %u", unformat_vnet_sw_interface, im->vnet_main,
       &sw_if_index, &spd_id))
    ;
  else if (unformat (line_input, "del"))
    is_add = 0;
  else
    {
      error = clib_error_return (0, "parse error: '%U'",
				 format_unformat_error, line_input);
      goto done;
    }

  ipsec_set_interface_spd (vm, sw_if_index, spd_id, is_add);

done:
  unformat_free (line_input);

  return error;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (set_interface_spd_command, static) = {
    .path = "set interface ipsec spd",
    .short_help =
    "set interface ipsec spd <int> <id>",
    .function = set_interface_spd_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
ipsec_sa_add_del_command_fn (vlib_main_t * vm,
			     unformat_input_t * input,
			     vlib_cli_command_t * cmd)
{
  unformat_input_t _line_input, *line_input = &_line_input;
  ip46_address_t tun_src = { }, tun_dst =
  {
  };
  ipsec_crypto_alg_t crypto_alg;
  ipsec_integ_alg_t integ_alg;
  ipsec_protocol_t proto;
  ipsec_sa_flags_t flags;
  clib_error_t *error;
  ipsec_key_t ck = { 0 };
  ipsec_key_t ik = { 0 };
  u32 id, spi, salt;
  int is_add, rv;

  error = NULL;
  is_add = 0;
  flags = IPSEC_SA_FLAG_NONE;
  proto = IPSEC_PROTOCOL_ESP;
  integ_alg = IPSEC_INTEG_ALG_NONE;
  crypto_alg = IPSEC_CRYPTO_ALG_NONE;

  if (!unformat_user (input, unformat_line_input, line_input))
    return 0;

  while (unformat_check_input (line_input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (line_input, "add %u", &id))
	is_add = 1;
      else if (unformat (line_input, "del %u", &id))
	is_add = 0;
      else if (unformat (line_input, "spi %u", &spi))
	;
      else if (unformat (line_input, "salt 0x%x", &salt))
	;
      else if (unformat (line_input, "esp"))
	proto = IPSEC_PROTOCOL_ESP;
      else if (unformat (line_input, "ah"))
	proto = IPSEC_PROTOCOL_AH;
      else if (unformat (line_input, "crypto-key %U",
			 unformat_ipsec_key, &ck))
	;
      else if (unformat (line_input, "crypto-alg %U",
			 unformat_ipsec_crypto_alg, &crypto_alg))
	;
      else if (unformat (line_input, "integ-key %U", unformat_ipsec_key, &ik))
	;
      else if (unformat (line_input, "integ-alg %U",
			 unformat_ipsec_integ_alg, &integ_alg))
	;
      else if (unformat (line_input, "tunnel-src %U",
			 unformat_ip46_address, &tun_src, IP46_TYPE_ANY))
	{
	  flags |= IPSEC_SA_FLAG_IS_TUNNEL;
	  if (!ip46_address_is_ip4 (&tun_src))
	    flags |= IPSEC_SA_FLAG_IS_TUNNEL_V6;
	}
      else if (unformat (line_input, "tunnel-dst %U",
			 unformat_ip46_address, &tun_dst, IP46_TYPE_ANY))
	;
      else if (unformat (line_input, "udp-encap"))
	flags |= IPSEC_SA_FLAG_UDP_ENCAP;
      else
	{
	  error = clib_error_return (0, "parse error: '%U'",
				     format_unformat_error, line_input);
	  goto done;
	}
    }

  if (is_add)
    rv = ipsec_sa_add_and_lock (id, spi, proto, crypto_alg,
				&ck, integ_alg, &ik, flags,
				0, clib_host_to_net_u32 (salt),
				&tun_src, &tun_dst, NULL);
  else
    rv = ipsec_sa_unlock_id (id);

  if (rv)
    error = clib_error_return (0, "failed");

done:
  unformat_free (line_input);

  return error;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (ipsec_sa_add_del_command, static) = {
    .path = "ipsec sa",
    .short_help =
    "ipsec sa [add|del]",
    .function = ipsec_sa_add_del_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
ipsec_spd_add_del_command_fn (vlib_main_t * vm,
			      unformat_input_t * input,
			      vlib_cli_command_t * cmd)
{
  unformat_input_t _line_input, *line_input = &_line_input;
  u32 spd_id = ~0;
  int is_add = ~0;
  clib_error_t *error = NULL;

  if (!unformat_user (input, unformat_line_input, line_input))
    return 0;

  while (unformat_check_input (line_input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (line_input, "add"))
	is_add = 1;
      else if (unformat (line_input, "del"))
	is_add = 0;
      else if (unformat (line_input, "%u", &spd_id))
	;
      else
	{
	  error = clib_error_return (0, "parse error: '%U'",
				     format_unformat_error, line_input);
	  goto done;
	}
    }

  if (spd_id == ~0)
    {
      error = clib_error_return (0, "please specify SPD ID");
      goto done;
    }

  ipsec_add_del_spd (vm, spd_id, is_add);

done:
  unformat_free (line_input);

  return error;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (ipsec_spd_add_del_command, static) = {
    .path = "ipsec spd",
    .short_help =
    "ipsec spd [add|del] <id>",
    .function = ipsec_spd_add_del_command_fn,
};
/* *INDENT-ON* */


static clib_error_t *
ipsec_policy_add_del_command_fn (vlib_main_t * vm,
				 unformat_input_t * input,
				 vlib_cli_command_t * cmd)
{
  unformat_input_t _line_input, *line_input = &_line_input;
  ipsec_policy_t p;
  int rv, is_add = 0;
  u32 tmp, tmp2, stat_index;
  clib_error_t *error = NULL;
  u32 is_outbound;

  clib_memset (&p, 0, sizeof (p));
  p.lport.stop = p.rport.stop = ~0;
  is_outbound = 0;

  if (!unformat_user (input, unformat_line_input, line_input))
    return 0;

  while (unformat_check_input (line_input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (line_input, "add"))
	is_add = 1;
      else if (unformat (line_input, "del"))
	is_add = 0;
      else if (unformat (line_input, "spd %u", &p.id))
	;
      else if (unformat (line_input, "inbound"))
	is_outbound = 0;
      else if (unformat (line_input, "outbound"))
	is_outbound = 1;
      else if (unformat (line_input, "priority %d", &p.priority))
	;
      else if (unformat (line_input, "protocol %u", &tmp))
	p.protocol = (u8) tmp;
      else
	if (unformat
	    (line_input, "action %U", unformat_ipsec_policy_action,
	     &p.policy))
	{
	  if (p.policy == IPSEC_POLICY_ACTION_RESOLVE)
	    {
	      error = clib_error_return (0, "unsupported action: 'resolve'");
	      goto done;
	    }
	}
      else if (unformat (line_input, "sa %u", &p.sa_id))
	;
      else if (unformat (line_input, "local-ip-range %U - %U",
			 unformat_ip4_address, &p.laddr.start.ip4,
			 unformat_ip4_address, &p.laddr.stop.ip4))
	;
      else if (unformat (line_input, "remote-ip-range %U - %U",
			 unformat_ip4_address, &p.raddr.start.ip4,
			 unformat_ip4_address, &p.raddr.stop.ip4))
	;
      else if (unformat (line_input, "local-ip-range %U - %U",
			 unformat_ip6_address, &p.laddr.start.ip6,
			 unformat_ip6_address, &p.laddr.stop.ip6))
	{
	  p.is_ipv6 = 1;
	}
      else if (unformat (line_input, "remote-ip-range %U - %U",
			 unformat_ip6_address, &p.raddr.start.ip6,
			 unformat_ip6_address, &p.raddr.stop.ip6))
	{
	  p.is_ipv6 = 1;
	}
      else if (unformat (line_input, "local-port-range %u - %u", &tmp, &tmp2))
	{
	  p.lport.start = tmp;
	  p.lport.stop = tmp2;
	}
      else
	if (unformat (line_input, "remote-port-range %u - %u", &tmp, &tmp2))
	{
	  p.rport.start = tmp;
	  p.rport.stop = tmp2;
	}
      else
	{
	  error = clib_error_return (0, "parse error: '%U'",
				     format_unformat_error, line_input);
	  goto done;
	}
    }

  rv = ipsec_policy_mk_type (is_outbound, p.is_ipv6, p.policy, &p.type);

  if (rv)
    {
      error = clib_error_return (0, "unsupported policy type for:",
				 " outboud:%s %s action:%U",
				 (is_outbound ? "yes" : "no"),
				 (p.is_ipv6 ? "IPv4" : "IPv6"),
				 format_ipsec_policy_action, p.policy);
      goto done;
    }

  rv = ipsec_add_del_policy (vm, &p, is_add, &stat_index);

  if (!rv)
    vlib_cli_output (vm, "policy-index:%d", stat_index);
  else
    vlib_cli_output (vm, "error:%d", rv);

done:
  unformat_free (line_input);

  return error;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (ipsec_policy_add_del_command, static) = {
    .path = "ipsec policy",
    .short_help =
    "ipsec policy [add|del] spd <id> priority <n> ",
    .function = ipsec_policy_add_del_command_fn,
};
/* *INDENT-ON* */

static void
ipsec_sa_show_all (vlib_main_t * vm, ipsec_main_t * im)
{
  u32 sai;

  /* *INDENT-OFF* */
  pool_foreach_index (sai, im->sad, ({
    vlib_cli_output(vm, "%U", format_ipsec_sa, sai, IPSEC_FORMAT_BRIEF);
  }));
  /* *INDENT-ON* */
}

static void
ipsec_spd_show_all (vlib_main_t * vm, ipsec_main_t * im)
{
  u32 spdi;

  /* *INDENT-OFF* */
  pool_foreach_index (spdi, im->spds, ({
    vlib_cli_output(vm, "%U", format_ipsec_spd, spdi);
  }));
  /* *INDENT-ON* */
}

static void
ipsec_spd_bindings_show_all (vlib_main_t * vm, ipsec_main_t * im)
{
  u32 spd_id, sw_if_index;
  ipsec_spd_t *spd;

  vlib_cli_output (vm, "SPD Bindings:");

  /* *INDENT-OFF* */
  hash_foreach(sw_if_index, spd_id, im->spd_index_by_sw_if_index, ({
    spd = pool_elt_at_index (im->spds, spd_id);
    vlib_cli_output (vm, "  %d -> %U", spd->id,
                     format_vnet_sw_if_index_name, im->vnet_main,
                     sw_if_index);
  }));
  /* *INDENT-ON* */
}

static void
ipsec_tunnel_show_all (vlib_main_t * vm, ipsec_main_t * im)
{
  u32 ti;

  vlib_cli_output (vm, "Tunnel interfaces");
  /* *INDENT-OFF* */
  pool_foreach_index (ti, im->tunnel_interfaces, ({
    vlib_cli_output(vm, "  %U", format_ipsec_tunnel, ti);
  }));
  /* *INDENT-ON* */
}

static clib_error_t *
show_ipsec_command_fn (vlib_main_t * vm,
		       unformat_input_t * input, vlib_cli_command_t * cmd)
{
  ipsec_main_t *im = &ipsec_main;

  ipsec_sa_show_all (vm, im);
  ipsec_spd_show_all (vm, im);
  ipsec_spd_bindings_show_all (vm, im);
  ipsec_tunnel_show_all (vm, im);

  return 0;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (show_ipsec_command, static) = {
    .path = "show ipsec all",
    .short_help = "show ipsec all",
    .function = show_ipsec_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
show_ipsec_sa_command_fn (vlib_main_t * vm,
			  unformat_input_t * input, vlib_cli_command_t * cmd)
{
  ipsec_main_t *im = &ipsec_main;
  u32 sai = ~0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "%u", &sai))
	;
      else
	break;
    }

  if (~0 == sai)
    ipsec_sa_show_all (vm, im);
  else
    vlib_cli_output (vm, "%U", format_ipsec_sa, sai, IPSEC_FORMAT_DETAIL);

  return 0;
}

static clib_error_t *
clear_ipsec_sa_command_fn (vlib_main_t * vm,
			   unformat_input_t * input, vlib_cli_command_t * cmd)
{
  ipsec_main_t *im = &ipsec_main;
  u32 sai = ~0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "%u", &sai))
	;
      else
	break;
    }

  if (~0 == sai)
    {
      /* *INDENT-OFF* */
      pool_foreach_index (sai, im->sad, ({
        ipsec_sa_clear(sai);
      }));
      /* *INDENT-ON* */
    }
  else
    {
      if (pool_is_free_index (im->sad, sai))
	return clib_error_return (0, "unknown SA index: %d", sai);
      else
	ipsec_sa_clear (sai);
    }

  return 0;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (show_ipsec_sa_command, static) = {
    .path = "show ipsec sa",
    .short_help = "show ipsec sa [index]",
    .function = show_ipsec_sa_command_fn,
};

VLIB_CLI_COMMAND (clear_ipsec_sa_command, static) = {
    .path = "clear ipsec sa",
    .short_help = "clear ipsec sa [index]",
    .function = clear_ipsec_sa_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
show_ipsec_spd_command_fn (vlib_main_t * vm,
			   unformat_input_t * input, vlib_cli_command_t * cmd)
{
  ipsec_main_t *im = &ipsec_main;
  u8 show_bindings = 0;
  u32 spdi = ~0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "%u", &spdi))
	;
      else if (unformat (input, "bindings"))
	show_bindings = 1;
      else
	break;
    }

  if (show_bindings)
    ipsec_spd_bindings_show_all (vm, im);
  else if (~0 != spdi)
    vlib_cli_output (vm, "%U", format_ipsec_spd, spdi);
  else
    ipsec_spd_show_all (vm, im);

  return 0;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (show_ipsec_spd_command, static) = {
    .path = "show ipsec spd",
    .short_help = "show ipsec spd [index]",
    .function = show_ipsec_spd_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
show_ipsec_tunnel_command_fn (vlib_main_t * vm,
			      unformat_input_t * input,
			      vlib_cli_command_t * cmd)
{
  ipsec_main_t *im = &ipsec_main;
  u32 ti = ~0;

  while (unformat_check_input (input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (input, "%u", &ti))
	;
      else
	break;
    }

  if (~0 != ti)
    vlib_cli_output (vm, "%U", format_ipsec_tunnel, ti);
  else
    ipsec_tunnel_show_all (vm, im);

  return 0;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (show_ipsec_tunnel_command, static) = {
    .path = "show ipsec tunnel",
    .short_help = "show ipsec tunnel [index]",
    .function = show_ipsec_tunnel_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
ipsec_show_backends_command_fn (vlib_main_t * vm,
				unformat_input_t * input,
				vlib_cli_command_t * cmd)
{
  ipsec_main_t *im = &ipsec_main;
  u32 verbose = 0;

  (void) unformat (input, "verbose %u", &verbose);

  vlib_cli_output (vm, "IPsec AH backends available:");
  u8 *s = format (NULL, "%=25s %=25s %=10s\n", "Name", "Index", "Active");
  ipsec_ah_backend_t *ab;
  /* *INDENT-OFF* */
  pool_foreach (ab, im->ah_backends, {
    s = format (s, "%=25s %=25u %=10s\n", ab->name, ab - im->ah_backends,
                ab - im->ah_backends == im->ah_current_backend ? "yes" : "no");
    if (verbose) {
        vlib_node_t *n;
        n = vlib_get_node (vm, ab->ah4_encrypt_node_index);
        s = format (s, "     enc4 %s (next %d)\n", n->name, ab->ah4_encrypt_next_index);
        n = vlib_get_node (vm, ab->ah4_decrypt_node_index);
        s = format (s, "     dec4 %s (next %d)\n", n->name, ab->ah4_decrypt_next_index);
        n = vlib_get_node (vm, ab->ah6_encrypt_node_index);
        s = format (s, "     enc6 %s (next %d)\n", n->name, ab->ah6_encrypt_next_index);
        n = vlib_get_node (vm, ab->ah6_decrypt_node_index);
        s = format (s, "     dec6 %s (next %d)\n", n->name, ab->ah6_decrypt_next_index);
    }
  });
  /* *INDENT-ON* */
  vlib_cli_output (vm, "%v", s);
  _vec_len (s) = 0;
  vlib_cli_output (vm, "IPsec ESP backends available:");
  s = format (s, "%=25s %=25s %=10s\n", "Name", "Index", "Active");
  ipsec_esp_backend_t *eb;
  /* *INDENT-OFF* */
  pool_foreach (eb, im->esp_backends, {
    s = format (s, "%=25s %=25u %=10s\n", eb->name, eb - im->esp_backends,
                eb - im->esp_backends == im->esp_current_backend ? "yes"
                                                                 : "no");
    if (verbose) {
        vlib_node_t *n;
        n = vlib_get_node (vm, eb->esp4_encrypt_node_index);
        s = format (s, "     enc4 %s (next %d)\n", n->name, eb->esp4_encrypt_next_index);
        n = vlib_get_node (vm, eb->esp4_decrypt_node_index);
        s = format (s, "     dec4 %s (next %d)\n", n->name, eb->esp4_decrypt_next_index);
        n = vlib_get_node (vm, eb->esp6_encrypt_node_index);
        s = format (s, "     enc6 %s (next %d)\n", n->name, eb->esp6_encrypt_next_index);
        n = vlib_get_node (vm, eb->esp6_decrypt_node_index);
        s = format (s, "     dec6 %s (next %d)\n", n->name, eb->esp6_decrypt_next_index);
    }
  });
  /* *INDENT-ON* */
  vlib_cli_output (vm, "%v", s);

  vec_free (s);
  return 0;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (ipsec_show_backends_command, static) = {
    .path = "show ipsec backends",
    .short_help = "show ipsec backends",
    .function = ipsec_show_backends_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
ipsec_select_backend_command_fn (vlib_main_t * vm,
				 unformat_input_t * input,
				 vlib_cli_command_t * cmd)
{
  unformat_input_t _line_input, *line_input = &_line_input;
  ipsec_main_t *im = &ipsec_main;
  clib_error_t *error;
  u32 backend_index;

  error = ipsec_rsc_in_use (im);

  if (error)
    return error;

  /* Get a line of input. */
  if (!unformat_user (input, unformat_line_input, line_input))
    return 0;

  if (unformat (line_input, "ah"))
    {
      if (unformat (line_input, "%u", &backend_index))
	{
	  if (ipsec_select_ah_backend (im, backend_index) < 0)
	    {
	      return clib_error_return (0, "Invalid AH backend index `%u'",
					backend_index);
	    }
	}
      else
	{
	  return clib_error_return (0, "Invalid backend index `%U'",
				    format_unformat_error, line_input);
	}
    }
  else if (unformat (line_input, "esp"))
    {
      if (unformat (line_input, "%u", &backend_index))
	{
	  if (ipsec_select_esp_backend (im, backend_index) < 0)
	    {
	      return clib_error_return (0, "Invalid ESP backend index `%u'",
					backend_index);
	    }
	}
      else
	{
	  return clib_error_return (0, "Invalid backend index `%U'",
				    format_unformat_error, line_input);
	}
    }
  else
    {
      return clib_error_return (0, "Unknown input `%U'",
				format_unformat_error, line_input);
    }

  return 0;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (ipsec_select_backend_command, static) = {
    .path = "ipsec select backend",
    .short_help = "ipsec select backend <ah|esp> <backend index>",
    .function = ipsec_select_backend_command_fn,
};

/* *INDENT-ON* */

static clib_error_t *
clear_ipsec_counters_command_fn (vlib_main_t * vm,
				 unformat_input_t * input,
				 vlib_cli_command_t * cmd)
{
  vlib_clear_combined_counters (&ipsec_spd_policy_counters);
  vlib_clear_combined_counters (&ipsec_sa_counters);

  return (NULL);
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (clear_ipsec_counters_command, static) = {
    .path = "clear ipsec counters",
    .short_help = "clear ipsec counters",
    .function = clear_ipsec_counters_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
create_ipsec_tunnel_command_fn (vlib_main_t * vm,
				unformat_input_t * input,
				vlib_cli_command_t * cmd)
{
  unformat_input_t _line_input, *line_input = &_line_input;
  ipsec_add_del_tunnel_args_t a;
  int rv;
  u32 num_m_args = 0;
  u8 ipv4_set = 0;
  u8 ipv6_set = 0;
  clib_error_t *error = NULL;
  ipsec_key_t rck = { 0 };
  ipsec_key_t lck = { 0 };
  ipsec_key_t lik = { 0 };
  ipsec_key_t rik = { 0 };

  clib_memset (&a, 0, sizeof (a));
  a.is_add = 1;

  /* Get a line of input. */
  if (!unformat_user (input, unformat_line_input, line_input))
    return 0;

  while (unformat_check_input (line_input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat
	  (line_input, "local-ip %U", unformat_ip46_address, &a.local_ip,
	   IP46_TYPE_ANY))
	{
	  ip46_address_is_ip4 (&a.local_ip) ? (ipv4_set = 1) : (ipv6_set = 1);
	  num_m_args++;
	}
      else
	if (unformat
	    (line_input, "remote-ip %U", unformat_ip46_address, &a.remote_ip,
	     IP46_TYPE_ANY))
	{
	  ip46_address_is_ip4 (&a.remote_ip) ? (ipv4_set = 1) : (ipv6_set =
								 1);
	  num_m_args++;
	}
      else if (unformat (line_input, "local-spi %u", &a.local_spi))
	num_m_args++;
      else if (unformat (line_input, "remote-spi %u", &a.remote_spi))
	num_m_args++;
      else if (unformat (line_input, "instance %u", &a.show_instance))
	a.renumber = 1;
      else if (unformat (line_input, "salt 0x%x", &a.salt))
	;
      else if (unformat (line_input, "udp-encap"))
	a.udp_encap = 1;
      else if (unformat (line_input, "use-esn"))
	a.esn = 1;
      else if (unformat (line_input, "use-anti-replay"))
	a.anti_replay = 1;
      else if (unformat (line_input, "tx-table %u", &a.tx_table_id))
	;
      else
	if (unformat
	    (line_input, "local-crypto-key %U", unformat_ipsec_key, &lck))
	;
      else
	if (unformat
	    (line_input, "remote-crypto-key %U", unformat_ipsec_key, &rck))
	;
      else if (unformat (line_input, "crypto-alg %U",
			 unformat_ipsec_crypto_alg, &a.crypto_alg))
	;
      else
	if (unformat
	    (line_input, "local-integ-key %U", unformat_ipsec_key, &lik))
	;
      else
	if (unformat
	    (line_input, "remote-integ-key %U", unformat_ipsec_key, &rik))
	;
      else if (unformat (line_input, "integ-alg %U",
			 unformat_ipsec_integ_alg, &a.integ_alg))
	;
      else if (unformat (line_input, "del"))
	a.is_add = 0;
      else
	{
	  error = clib_error_return (0, "unknown input `%U'",
				     format_unformat_error, line_input);
	  goto done;
	}
    }

  if (num_m_args < 4)
    {
      error = clib_error_return (0, "mandatory argument(s) missing");
      goto done;
    }

  if (ipv4_set && ipv6_set)
    return clib_error_return (0, "both IPv4 and IPv6 addresses specified");

  a.is_ip6 = ipv6_set;

  clib_memcpy (a.local_crypto_key, lck.data, lck.len);
  a.local_crypto_key_len = lck.len;
  clib_memcpy (a.remote_crypto_key, rck.data, rck.len);
  a.remote_crypto_key_len = rck.len;

  clib_memcpy (a.local_integ_key, lik.data, lik.len);
  a.local_integ_key_len = lck.len;
  clib_memcpy (a.remote_integ_key, rik.data, rik.len);
  a.remote_integ_key_len = rck.len;

  rv = ipsec_add_del_tunnel_if (&a);

  switch (rv)
    {
    case 0:
      break;
    case VNET_API_ERROR_INVALID_VALUE:
      if (a.is_add)
	error = clib_error_return (0,
				   "IPSec tunnel interface already exists...");
      else
	error = clib_error_return (0, "IPSec tunnel interface not exists...");
      goto done;
    default:
      error = clib_error_return (0, "ipsec_register_interface returned %d",
				 rv);
      goto done;
    }

done:
  unformat_free (line_input);

  return error;
}

/* *INDENT-OFF* */
VLIB_CLI_COMMAND (create_ipsec_tunnel_command, static) = {
  .path = "create ipsec tunnel",
  .short_help = "create ipsec tunnel local-ip <addr> local-spi <spi> "
      "remote-ip <addr> remote-spi <spi> [instance <inst_num>] [udp-encap] [use-esn] [use-anti-replay] "
      "[tx-table <table-id>]",
  .function = create_ipsec_tunnel_command_fn,
};
/* *INDENT-ON* */

static clib_error_t *
ipsec_tun_protect_cmd (vlib_main_t * vm,
		       unformat_input_t * input, vlib_cli_command_t * cmd)
{
  unformat_input_t _line_input, *line_input = &_line_input;
  u32 sw_if_index, is_del, sa_in, sa_out, *sa_ins = NULL;
  vnet_main_t *vnm;

  is_del = 0;
  sw_if_index = ~0;
  vnm = vnet_get_main ();

  if (!unformat_user (input, unformat_line_input, line_input))
    return 0;

  while (unformat_check_input (line_input) != UNFORMAT_END_OF_INPUT)
    {
      if (unformat (line_input, "del"))
	is_del = 1;
      else if (unformat (line_input, "add"))
	is_del = 0;
      else if (unformat (line_input, "sa-in %d", &sa_in))
	vec_add1 (sa_ins, sa_in);
      else if (unformat (line_input, "sa-out %d", &sa_out))
	;
      else if (unformat (line_input, "%U",
			 unformat_vnet_sw_interface, vnm, &sw_if_index))
	;
      else
	return (clib_error_return (0, "unknown input '%U'",
				   format_unformat_error, line_input));
    }

  if (!is_del)
    ipsec_tun_protect_update (sw_if_index, sa_out, sa_ins);

  unformat_free (line_input);
  return NULL;
}

/**
 * Protect tunnel with IPSEC
 */
/* *INDENT-OFF* */
VLIB_CLI_COMMAND (ipsec_tun_protect_cmd_node, static) =
{
  .path = "ipsec tunnel protect",
  .function = ipsec_tun_protect_cmd,
  .short_help = "ipsec tunnel protect <interface> input-sa <SA> output-sa <SA>",
    // this is not MP safe
};
/* *INDENT-ON* */

static walk_rc_t
ipsec_tun_protect_show_one (index_t itpi, void *ctx)
{
  vlib_cli_output (ctx, "%U", format_ipsec_tun_protect, itpi);

  return (WALK_CONTINUE);
}

static clib_error_t *
ipsec_tun_protect_show (vlib_main_t * vm,
			unformat_input_t * input, vlib_cli_command_t * cmd)
{
  ipsec_tun_protect_walk (ipsec_tun_protect_show_one, vm);

  return NULL;
}

/**
 * show IPSEC tunnel protection
 */
/* *INDENT-OFF* */
VLIB_CLI_COMMAND (ipsec_tun_protect_show_node, static) =
{
  .path = "show ipsec protect",
  .function = ipsec_tun_protect_show,
  .short_help =  "show ipsec protect",
};
/* *INDENT-ON* */

clib_error_t *
ipsec_cli_init (vlib_main_t * vm)
{
  return 0;
}

VLIB_INIT_FUNCTION (ipsec_cli_init);


/*
 * fd.io coding-style-patch-verification: ON
 *
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
