#define DEBUG_AUTH
#include "cdvws.h"

//#define AUTH_MSG_SLEEP

#define HTTP_UNAUTHORIZED 1
#define HTTP_INTERNAL_SERVER_ERROR 2
#define OK 3

/* For Kerberos5 & GSSAPI */
#include <gssapi.h>
#include <krb5.h>
#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>

#ifdef DEBUG_AUTH
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/auth-handler] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

#ifdef USE_KERBEROS
void set_gss_error(const char *func, OM_uint32 err_maj, OM_uint32 err_min) {
	OM_uint32 maj_stat, min_stat;
	OM_uint32 msg_ctx = 0;
	gss_buffer_desc status_string;
	char buf_maj[512] = { 0 };
	char buf_min[512] = { 0 };

	do {
		maj_stat = gss_display_status (&min_stat,
						err_maj,
						GSS_C_GSS_CODE,
						GSS_C_NO_OID,
						&msg_ctx,
						&status_string);
		if (GSS_ERROR(maj_stat))
			break;

		strncpy(buf_maj, (char*) status_string.value, sizeof(buf_maj));
		gss_release_buffer(&min_stat, &status_string);

		maj_stat = gss_display_status (&min_stat,
						err_min,
						GSS_C_MECH_CODE,
						GSS_C_NULL_OID,
						&msg_ctx,
						&status_string);

		if (!GSS_ERROR(maj_stat)) {
			strncpy(buf_min, (char*) status_string.value, sizeof(buf_min));
			gss_release_buffer(&min_stat, &status_string);
		}
	} while (!GSS_ERROR(maj_stat) && msg_ctx != 0);

	DPRINTF("%s: GSS-API Error (%d, %d) => %s, %s\n", func, err_maj, err_min, buf_maj, buf_min);
}

char *krb5_server_auth(char *service, char *challenge) {
	OM_uint32 maj_stat;
	OM_uint32 min_stat;
	gss_buffer_desc name_token = GSS_C_EMPTY_BUFFER;
	gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
	gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
	gss_cred_id_t server_creds = GSS_C_NO_CREDENTIAL;
	gss_cred_id_t client_creds = GSS_C_NO_CREDENTIAL;
	gss_name_t server_name = GSS_C_NO_NAME;
	gss_name_t client_name = GSS_C_NO_NAME;
	gss_ctx_id_t context = GSS_C_NO_CONTEXT;
	char *username = NULL;
	char *response = NULL;

	/* Init */
	name_token.length = strlen(service);
	name_token.value = (char *)service;

	maj_stat = gss_import_name(&min_stat, &name_token, GSS_C_NT_HOSTBASED_SERVICE, &server_name);
	if (GSS_ERROR(maj_stat)) {
		set_gss_error(__FUNCTION__, maj_stat, min_stat);
		return NULL;
	}

	DPRINTF("C: %s\n", challenge);

	/* Step */
	size_t len = -1;
	input_token.value = base64_decode(challenge, &len);
	input_token.length = len;

	gss_ctx_id_t context_hdl = GSS_C_NO_CONTEXT;
	do {
		//receive_token_from_peer(input_token);
		//TODO: http://www.opensource.apple.com/source/Heimdal/Heimdal-172.27/lib/krb5/test_keytab.c
		//krb5_kt_resolve(context, kt_pathname, &keytab);
		maj_stat = gss_accept_sec_context(&min_stat,
					&context_hdl,
                                        server_creds, // cred_hdl,
					&input_token,
					GSS_C_NO_CHANNEL_BINDINGS,
					&client_name,
					NULL,
					&output_token,
					NULL,
					NULL,
					&client_creds);
		if (GSS_ERROR(maj_stat)) {
			set_gss_error(__FUNCTION__, maj_stat, min_stat);
			return NULL;
		}

		if (GSS_ERROR(maj_stat)) {
			set_gss_error(__FUNCTION__, maj_stat, min_stat);
			if (context_hdl != GSS_C_NO_CONTEXT)
				gss_delete_sec_context(&min_stat,
						&context_hdl,
						GSS_C_NO_BUFFER);
			break;
		}
	} while (maj_stat & GSS_S_CONTINUE_NEEDED);

	maj_stat = gss_display_name(&min_stat, client_name, &output_token, NULL);
	if (GSS_ERROR(maj_stat)) {
		set_gss_error(__FUNCTION__, maj_stat, min_stat);
		return NULL;
	}

	username = (char *)utils_alloc("handler_auth.server_auth.username", output_token.length + 1);
	strncpy(username, (char*) output_token.value, output_token.length);
	username[output_token.length] = 0;

	/* Free */
	if (context != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
	if (server_name != GSS_C_NO_NAME)
		gss_release_name(&min_stat, &server_name);
	if (response != NULL)
		response = utils_free("handler-auth.krb5_server_auth.response", response);

	return strdup(username);
}

unsigned char *krb5_client_ticket(char *service) {
	OM_uint32 maj_stat;
	OM_uint32 min_stat;
	gss_buffer_desc name_token = GSS_C_EMPTY_BUFFER;
	gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
	gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
	gss_name_t server_name = GSS_C_NO_NAME;
        gss_ctx_id_t context = GSS_C_NO_CONTEXT;
	unsigned char *response = NULL;
	char *username = NULL;

	name_token.length = strlen(service);
	name_token.value = (char *)service;

	maj_stat = gss_import_name(&min_stat, &name_token, gss_krb5_nt_service_name, &server_name);
	if (GSS_ERROR(maj_stat)) {
		set_gss_error(__FUNCTION__, maj_stat, min_stat);
		return NULL;
	}

	maj_stat = gss_init_sec_context(&min_stat,
		GSS_C_NO_CREDENTIAL,
		&context,
		server_name,
		GSS_C_NO_OID,
		GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG,
		0,
		GSS_C_NO_CHANNEL_BINDINGS,
		&input_token,
		NULL,
		&output_token,
		NULL,
		NULL);

	if ((maj_stat != GSS_S_COMPLETE) && (maj_stat != GSS_S_CONTINUE_NEEDED)) {
		set_gss_error(__FUNCTION__, maj_stat, min_stat);
		return NULL;
	}

	if (output_token.length) {
		size_t sz = output_token.length;

		response = base64_encode((const char *)output_token.value, &sz);
		gss_release_buffer(&min_stat, &output_token);
	}

	if (maj_stat == GSS_S_COMPLETE) {
		gss_name_t gssuser = GSS_C_NO_NAME;
		maj_stat = gss_inquire_context(&min_stat, context, &gssuser, NULL, NULL, NULL,  NULL, NULL, NULL);
		if (GSS_ERROR(maj_stat)) {
			set_gss_error(__FUNCTION__, maj_stat, min_stat);
			return NULL;
		}

		name_token.length = 0;
		maj_stat = gss_display_name(&min_stat, gssuser, &name_token, NULL);
		if (GSS_ERROR(maj_stat)) {
			set_gss_error(__FUNCTION__, maj_stat, min_stat);
			return NULL;
		}

		username = (char *)utils_alloc("handler-auth.client_ticket.username", name_token.length + 1);
		strncpy(username, (char*) name_token.value, name_token.length);
		username[name_token.length] = 0;
		gss_release_buffer(&min_stat, &name_token);
		gss_release_name(&min_stat, &gssuser);
	}

	if (output_token.value)
		gss_release_buffer(&min_stat, &output_token);
	if (input_token.value)
		input_token.value = utils_free("handler-auth.client-ticket.input-token", input_token.value);

	return response;
}

int get_gss_creds(char *krb_service_name, gss_cred_id_t *server_creds) {
	gss_buffer_desc token = GSS_C_EMPTY_BUFFER;
	OM_uint32 major_status, minor_status, minor_status2;
	gss_name_t server_name = GSS_C_NO_NAME;
	char buf[1024];
	int have_server_princ;


	have_server_princ = krb_service_name && strchr(krb_service_name, '/') != NULL;
	if (have_server_princ)
		strncpy(buf, krb_service_name, sizeof(buf));
	else if (krb_service_name && strcmp(krb_service_name,"Any") == 0) {      
		*server_creds = GSS_C_NO_CREDENTIAL;
		return 0;
	}
	else
		return 1;

	token.value = buf;
	token.length = strlen(buf) + 1;

	major_status = gss_import_name(&minor_status, &token,
				(have_server_princ) ? (gss_OID) GSS_KRB5_NT_PRINCIPAL_NAME : (gss_OID) GSS_C_NT_HOSTBASED_SERVICE,
				&server_name);

	memset(&token, 0, sizeof(token));
	if (GSS_ERROR(major_status)) {
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	major_status = gss_display_name(&minor_status, server_name, &token, NULL);
	if (GSS_ERROR(major_status)) {
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	gss_release_buffer(&minor_status, &token);

	major_status = gss_acquire_cred(&minor_status, server_name, GSS_C_INDEFINITE,
			GSS_C_NO_OID_SET, GSS_C_ACCEPT,
			server_creds, NULL, NULL);

	gss_release_name(&minor_status2, &server_name);
	if (GSS_ERROR(major_status)) {
		return HTTP_INTERNAL_SERVER_ERROR;
	}
   
	return 0;
}

char *krb5_get_user(char *keytab, char *srv_princ, const char *auth_line) {
	OM_uint32 major_status, minor_status;
	gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
	gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
	gss_name_t client_name = GSS_C_NO_NAME;
	gss_cred_id_t delegated_cred = GSS_C_NO_CREDENTIAL;
	gss_ctx_id_t context = GSS_C_NO_CONTEXT;
	gss_cred_id_t server_creds = GSS_C_NO_CREDENTIAL;
	OM_uint32 ret_flags = 0;
	char *user = NULL;
	size_t len;
	int ret;

	if (keytab) {
		char *ktname = NULL;

		DPRINTF("%s: Using keytab from %s\n", __FUNCTION__, keytab);

		ktname = utils_alloc("handler-auth.krb5_get_user.ktname", strlen("KRB5_KTNAME=") + strlen(keytab) + 1);
		if (ktname == NULL)
			goto end;

		sprintf(ktname, "KRB5_KTNAME=%s", keytab);
		putenv(ktname);
	}

	DPRINTF("%s: Getting credentials for principal %s\n", __FUNCTION__, srv_princ);
	ret = get_gss_creds(srv_princ, &server_creds);
	if (ret)
		goto end;

	DPRINTF("Auth_line is %s\n", auth_line);

	#ifdef AUTH_MSG_SLEEP
	DPRINTF("***** To debug this process please run: gdb -p %d *****\n", getpid());
	sleep(10);
	#else
	sleep(1);
	#endif

	len = strlen(auth_line);
	input_token.value = base64_decode(auth_line, &len);
	input_token.length = len;

	major_status = gss_accept_sec_context(&minor_status,
					  &context,
					  server_creds,
					  &input_token,
					  GSS_C_NO_CHANNEL_BINDINGS,
					  &client_name,
					  NULL,
					  &output_token,
					  &ret_flags,
					  NULL,
					  &delegated_cred);

	DPRINTF("%s: Client %s us their credential\n", __FUNCTION__,
			(ret_flags & GSS_C_DELEG_FLAG) ? "delegated" : "didn't delegate");

	if (GSS_ERROR(major_status)) {
		if (input_token.length > 7 && memcmp(input_token.value, "NTLMSSP", 7) == 0)
			DPRINTF("%s: Authorization type is not supported\n", __FUNCTION__);
		goto end;
	}

	major_status = gss_display_name(&minor_status, client_name, &output_token, NULL);
	gss_release_name(&minor_status, &client_name); 
	if (GSS_ERROR(major_status))
		goto end;

	user = strdup(output_token.value);

	gss_release_buffer(&minor_status, &output_token);
end:
	if (delegated_cred)
		gss_release_cred(&minor_status, &delegated_cred);

	if (output_token.length) 
		gss_release_buffer(&minor_status, &output_token);

	if (client_name != GSS_C_NO_NAME)
		gss_release_name(&minor_status, &client_name);

	if (server_creds != GSS_C_NO_CREDENTIAL)
		gss_release_cred(&minor_status, &server_creds);

	if (context != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&minor_status, &context, GSS_C_NO_BUFFER);

	return user;
}
#endif

char *http_get_authorization(char *buf, char *host, char *keytab, char *kerb_realm) {
	char *ret = NULL;
	int idx = -1;
	tTokenizer t;
	int i;

	t = tokenize(buf, "\n");
	for (i = 0; i < t.numTokens; i++) {
		if (strlen(t.tokens[i]) < 2)
			break;
		if (strstr(t.tokens[i], "Authorization:") != NULL)
			idx = i;
	}

	if (idx == -1) {
		DPRINTF("%s: No authorization token found\n", __FUNCTION__);
		free_tokens(t);
		return NULL;//strdup("REQAUTH");
	}

	tTokenizer t2 = tokenize(t.tokens[idx], " ");
	#ifdef USE_KERBEROS
	if ((strcmp(t2.tokens[1], "Negotiate") == 0) && (kerb_realm != NULL)) {
		char tmp[4096] = { 0 };
		char kt[4096] = { 0 };

		getcwd(tmp, sizeof(tmp));
		snprintf(kt, sizeof(kt), "%s/%s/%s", tmp, _proj_root_dir, keytab);

		DPRINTF("%s: Performing Negotation using Kerberos 5\n", __FUNCTION__);
		snprintf(tmp, sizeof(tmp), "HTTP/%s@%s", host, kerb_realm);
		ret = krb5_get_user(kt, tmp, t2.tokens[2]);

		DPRINTF("Kerberos 5 username is %s\n", ret ? ret : "empty");
	}
	else
	#endif
	if (strcmp(t2.tokens[1], "Basic") == 0) {
		char *password = NULL;

		size_t len = strlen(t2.tokens[2]);
		char *auth = base64_decode(t2.tokens[2], &len);

		if (len > 0)
			auth[len] = 0;

		password = strchr(auth, ':');
		if (password != NULL) {
			char user[1024] = { 0 };
			memcpy(user, auth, strlen(auth) - strlen(password) );
			*password++;

			DPRINTF("Auth: %s (user '%s', password '%s')\n", auth, user, password);
		}
		else
			DPRINTF("%s: No valid authorization data\n", __FUNCTION__);
	}
	free_tokens(t2);
	free_tokens(t);

	return ret;
}

