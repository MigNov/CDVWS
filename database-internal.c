#define DEBUG_IDB

#include "cdvws.h"

#ifdef USE_INTERNAL_DB

#ifdef DEBUG_IDB
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/database-idb] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

long _idb_get_field_id(char *table, char *field);
long _idb_table_id(char *name);
int _idb_get_table_field_count(long id);
int _idb_get_type_id_from_field(long idField);

int idb_init(void)
{
	idb_tables_num = 0;
	idb_fields_num = 0;
	idb_tabdata_num = 0;

	idb_tables = malloc( sizeof(tTableDef) );
	idb_fields = malloc( sizeof(tTableFieldDef) );
	idb_tabdata = malloc( sizeof(tTableData) );

	_idb_filename = NULL;
	_idb_datadir  = NULL;
	_idb_querylog = NULL;

	_idb_mincrypt_enabled = 0;

	_idb_num_queries = 0;
	_idb_num_queries_create = 0;
	_idb_num_queries_drop = 0;
	_idb_num_queries_insert = 0;
	_idb_num_queries_update = 0;
	_idb_num_queries_delete = 0;
	_idb_num_queries_select = 0;

	/* Initialize session timer */
	_idb_session_start.tv_sec  = 0;
	_idb_session_start.tv_nsec = 0;

	/* Initialize query timer */
	_idb_last_ts.tv_sec  = 0;
	_idb_last_ts.tv_nsec = 0;

	/* Initialize the NO RESULT type */
	tdsNone.num_rows = -1;
	tdsNone.rows = NULL;

	if ((idb_tables != NULL) && (idb_fields != NULL)
		&& (idb_tabdata != NULL))
			return 0;

	return -ENOMEM;
}

struct timespec idb_get_time(int diff)
{
	return utils_get_time(diff);
}

char *idb_get_filename(void)
{
	return _idb_filename;
}

char *_idb_get_input_value(char *data, int *otype)
{
	int i, type;
	char *out = NULL;
	tTokenizer t;

	if (strncmp(data, "'", 1) == 0)
		type = IDB_TYPE_STR;
	else
	if (atol(data) != atoi(data))
		type = IDB_TYPE_LONG;
	else
	if (atol(data) == atoi(data))
		type = IDB_TYPE_INT;
	else
		type = 0;

	t = tokenize(data, "'");
	for (i = 0; i < t.numTokens; i++)
		out = strdup(t.tokens[i]);

	if (otype != NULL)
		*otype = type;

	free_tokens(t);
	return out;
}

int idb_query(char *query)
{
	int i, ret = -ENOTSUP;
	struct timespec tss = { .tv_sec = 0, .tv_nsec = 0 };
	struct timespec ts, tse;
	float myTime = -1;
	tTokenizer t;
	int log = 0;

	if (_perf_measure)
		tss = utils_get_time( TIME_CURRENT );

	if (query == NULL) {
		ret = -EINVAL;
		goto cleanup;
	}

	/* Log queries to file is file is set */
	if ((_idb_querylog != NULL) && (strcmp(query, "CLOSE") != 0)) {
		int fd;
		char tmp[4096] = { 0 };

		snprintf(tmp, sizeof(tmp), "Query \"%s", query);
		fd = open(_idb_querylog, O_WRONLY | O_CREAT | O_APPEND, 0644);
		write(fd, tmp, strlen(tmp));
		close(fd);

		log = 1;
	}

	if ((query != NULL) && (strlen(query) > 0) && (query[strlen(query) - 1] == ';'))
		query[strlen(query) - 1] = 0;

	DPRINTF("%s: Processing '%s'\n", __FUNCTION__, query);
	idb_get_time( TIME_CURRENT );

	if (strncmp(query, "INIT", 4) == 0) {
		t = tokenize(query, " ");

		if (strcmp(t.tokens[0], "INIT") == 0) {
			if (_idb_datadir == NULL)
				ret = idb_load(t.tokens[1]);
			else {
				char path[4096] = { 0 };
				snprintf(path, sizeof(path), "%s/%s", _idb_datadir, t.tokens[1]);

				ret = idb_load(path);
			}
		}

		free_tokens(t);
	}
	else
	if (strncmp(query, "REINIT", 6) == 0) {
		t = tokenize(query, " ");

		if (strcmp(t.tokens[0], "REINIT") == 0) {
			if (_idb_datadir == NULL) {
				unlink(t.tokens[1]);
				ret = idb_load(t.tokens[1]);
			}
			else {
				char path[4096] = { 0 };
				snprintf(path, sizeof(path), "%s/%s", _idb_datadir, t.tokens[1]);

				unlink(path);
				ret = idb_load(path);
			}
		}
	}
	else
	if (strncmp(query, "SET ", 4) == 0) {
		t = tokenize(query, " ");

		if (t.numTokens < 3)
			return -EINVAL;

		if (strcmp(t.tokens[1], "DATADIR") == 0) {
			_idb_datadir = strdup(t.tokens[2]);

			DPRINTF("%s: Data directory set to '%s'\n", __FUNCTION__,
				_idb_datadir);

			ret = 0;
		}
		else
		if ((strcmp(t.tokens[1], "QUERYLOG") == 0)
			|| (strcmp(t.tokens[1], "FRESHQUERYLOG") == 0)) {
			_idb_querylog = strdup(t.tokens[2]);

			/* Start the fresh log if appropriate */
			if (strcmp(t.tokens[1], "FRESHQUERYLOG") == 0)
				unlink(_idb_querylog);

			DPRINTF("%s: Setting up query logging to '%s'\n",
				__FUNCTION__, _idb_querylog);

			ret = 0;
		}
		else
		if (strcmp(t.tokens[1], "FILENAME") == 0) {
			_idb_filename = strdup( t.tokens[2] );

			ret = 0;
		}
		else
		if (strcmp(t.tokens[1], "MINCRYPT") == 0) {
			if (t.numTokens < 4)
				ret = -EINVAL;
			else {
				long ver = wrap_mincrypt_get_version();
				if (ver == -1)
					DPRINTF("%s: Not compiled against minCrypt library\n",
						__FUNCTION__);
				else {
					DPRINTF("%s: Found minCrypt version %d.%d.%d\n", __FUNCTION__,
						(int)((ver >> 16) & 0xFF), (int)((ver >> 8 & 0xFF)),
						(int)(ver & 0xFF));

					if (wrap_mincrypt_set_password(t.tokens[2], t.tokens[3]) == 0) {
						DPRINTF("%s: minCrypt initialization vectors set, enabling.\n",
							__FUNCTION__);
						_idb_mincrypt_enabled = 1;
					}

					ret = 0;
				}
				
			}
		}

		free_tokens(t);
	}
	else
	if (strncmp(query, "CREATE TABLE", 12) == 0) {
		char *cm = NULL;
		char *name = NULL;
		char *data = NULL;
		tTokenizer t2;
		tTableFieldDef *fd = NULL;

		t = tokenize(query + 13, "(");
		name = strdup(t.tokens[0]);
		data = strdup(t.tokens[1]);
		free_tokens(t);

		t = tokenize(data, ")");
		free(data); data = NULL;
		data = strdup(t.tokens[0]);
		free_tokens(t);
		t = tokenize(data, ",");

		fd = malloc( t.numTokens * sizeof(tTableFieldDef) );
		memset(fd, 0, t.numTokens * sizeof(tTableFieldDef));

		for (i = 0; i < t.numTokens; i++) {
			t2 = tokenize(trim(t.tokens[i]), " ");
			if (t2.numTokens == 2) {
				fd[i].name = strdup( trim(t2.tokens[0]) );
				fd[i].type = idb_type_id(trim(t2.tokens[1]));
			}
		}

		if (strstr(query, "COMMENT") != NULL) {
			t2 = tokenize( strstr(query, "COMMENT") + 9, "'" );
			cm = t2.tokens[0];
		}
		ret = idb_table_create(name, t.numTokens, fd, cm);
		free_tokens(t2);
		free_tokens(t);
	}
	else
	if (strncmp(query, "INSERT INTO", 11) == 0) {
		char *name = NULL;

		t = tokenize(query, " ");
		if ((t.numTokens > 3)
			&& (strcmp(t.tokens[0], "INSERT") == 0)
			&& (strcmp(t.tokens[1], "INTO") == 0)) {
				tTokenizer t2;
				char **flddata = NULL;

				char *tmp = strdup(t.tokens[2]);
				free_tokens(t);

				t = tokenize(tmp, "(");
				name = strdup( trim(t.tokens[0]) );
				free_tokens(t);
				free(tmp); tmp = NULL;

				t = tokenize(query + 11, "(");
				if (t.numTokens != 3)
					ret = -EINVAL;
				else {
					flddata = (char **)malloc( 2 * sizeof(char *) );
					for (i = 1; i < t.numTokens; i++) {
						t2 = tokenize(t.tokens[i], ")");
						flddata[i - 1] = strdup( trim(t2.tokens[0]) );
						free_tokens(t2);
					}

					DPRINTF("%s: Table name is '%s'\n", __FUNCTION__, name);
					free_tokens(t);

					t = tokenize(flddata[0], ",");
					t2 = tokenize(flddata[1], ",");

					if ((t.numTokens != t2.numTokens)
						|| (t.numTokens != _idb_get_table_field_count( _idb_table_id(name)))) {
						DPRINTF("%s: Invalid number of fields = { t1: %d, t2: %d, tab: %d }\n", __FUNCTION__,
							t.numTokens, t2.numTokens, _idb_get_table_field_count( _idb_table_id(name) ));
						ret = -EINVAL;
					}
					else {
						tTableDataInput *tdi = NULL;

						DPRINTF("%s: All columns of '%s' are about to be filled\n", __FUNCTION__, name);

						/* Prepare row for insertion */
						tdi = malloc( t.numTokens * sizeof(tTableDataInput) );
						memset(tdi, 0, t.numTokens * sizeof(tTableDataInput) );

						for (i = 0; i < t.numTokens; i++) {
							int type;
							char *tmp2 = NULL;

							tmp2 = _idb_get_input_value(trim(t2.tokens[i]), &type);

							tdi[i].name = strdup( _idb_get_input_value(trim(t.tokens[i]), NULL) );
							if (type == IDB_TYPE_INT)
								tdi[i].iValue = atoi(tmp2);
							else
							if (type == IDB_TYPE_LONG)
								tdi[i].lValue = atol(tmp2);
							else
							if (type == IDB_TYPE_STR)
								tdi[i].sValue = strdup(tmp2);

							free(tmp2); tmp2 = NULL;
						}

						ret = idb_table_insert(name, t.numTokens, tdi);
					}
				}
			free_tokens(t2);
			free_tokens(t);
		}
		else {
			free_tokens(t);
			DPRINTF("%s: Syntax error\n", __FUNCTION__);
		}
	}
	else
	if (strncmp(query, "UPDATE", 6) == 0) {
		t = tokenize(query, " ");
		if ((t.numTokens < 3) || (strcmp(t.tokens[2], "SET") != 0))
			ret = -EINVAL;
		else {
			int len = -1;
			int num_fields;
			tTokenizer t2;
			int nextIsField;
			int num_where_fields = 0;
			tTableDataInput *tdi = NULL;
			tTableDataInput *where_fields = NULL;
			char *name = strdup(t.tokens[1]);

			free_tokens(t);

			if (strstr(query, "WHERE ") != NULL) {
				char *tmp = strstr(query, "WHERE ") + 6;

				where_fields = (tTableDataInput *)malloc( sizeof(tTableDataInput) );
				memset(where_fields, 0, sizeof(tTableDataInput));
				num_where_fields = 0;

				t2 = tokenize(tmp, " ");
				nextIsField = 1;
				for (i = 0; i < t2.numTokens; i++) {
					if ((nextIsField == 1) || (i == 0)) {
						num_where_fields++;
						where_fields = (tTableDataInput *)realloc( where_fields,
							num_where_fields * sizeof(tTableDataInput) );

						where_fields[num_where_fields - 1].name = strdup(t2.tokens[i]);

						nextIsField = -1;
					}

					if ((strcmp(t2.tokens[i], "AND") == 0)
						|| (strcmp(t2.tokens[i], "OR") == 0))
						nextIsField = 1;
					else {
						if (nextIsField != -1) {
							if (strcmp(t2.tokens[i], "=") != 0) {
								char *tmp2 = NULL;
								int type;

								long idField = _idb_get_field_id(name,
									where_fields[num_where_fields - 1].name);
								tmp2 = _idb_get_input_value(trim(t2.tokens[i]), NULL);
								type = _idb_get_type_id_from_field(idField);

								if (type == IDB_TYPE_INT)
									where_fields[num_where_fields - 1].iValue = atoi(tmp2);
								else
								if (type == IDB_TYPE_LONG)
									where_fields[num_where_fields - 1].lValue = atol(tmp2);
								else
								if (type == IDB_TYPE_STR)
									where_fields[num_where_fields - 1].sValue = strdup(tmp2);

								free(tmp2); tmp2 = NULL;
							}
						}
						nextIsField = 0;
					}
				}
				free_tokens(t2);

				len = strlen(query) - strlen(tmp) - 6;
			}

			if (len > -1) {
				char *tmp = strdup(query);
				DPRINTF("%s: Update data ends on position %d\n", __FUNCTION__, len);
				tmp[len] = 0;

				query = strdup(tmp);
			}

			t = tokenize(query + 12 + strlen(name), ",");

			/* Prepare row for update */
			tdi = malloc( t.numTokens * sizeof(tTableDataInput) );
			memset(tdi, 0, t.numTokens * sizeof(tTableDataInput) );

			num_fields = t.numTokens;
			for (i = 0; i < t.numTokens; i++) {
				/* TODO: Fix when found in string */
				t2 = tokenize(trim(t.tokens[i]), "=");
				if (t2.numTokens == 2) {
					int type;
					char *tmp2 = NULL;

					tmp2 = _idb_get_input_value(trim(t2.tokens[1]), &type);

					tdi[i].name = strdup( _idb_get_input_value(trim(t2.tokens[0]), NULL) );
					if (type == IDB_TYPE_INT)
						tdi[i].iValue = atoi(tmp2);
					else
					if (type == IDB_TYPE_LONG)
						tdi[i].lValue = atol(tmp2);
					else
					if (type == IDB_TYPE_STR)
						tdi[i].sValue = strdup(tmp2);

					free(tmp2); tmp2 = NULL;
				}
				else
					DPRINTF("%s: Incorrect data field token count\n", __FUNCTION__);
				free_tokens(t2);
			}

			ret = (idb_table_update(name, num_fields, tdi, num_where_fields, where_fields) >= 0) ? 0 : -ENOENT;
		}
		free_tokens(t);
	}
	else
	if (strncmp(query, "DELETE FROM", 11) == 0) {
		tTokenizer t2;
		int nextIsField;
		int num_where_fields = 0;
		tTableDataInput *where_fields = NULL;
		char *name = NULL;

		t = tokenize(query, " ");
		name = strdup(t.tokens[2]);

		if (strstr(query, "WHERE ") != NULL) {
			char *tmp = strstr(query, "WHERE ") + 6;

			where_fields = (tTableDataInput *)malloc( sizeof(tTableDataInput) );
			memset(where_fields, 0, sizeof(tTableDataInput));
			num_where_fields = 0;

			t2 = tokenize(tmp, " ");
			nextIsField = 1;
			for (i = 0; i < t2.numTokens; i++) {
				if ((nextIsField == 1) || (i == 0)) {
					num_where_fields++;
					where_fields = (tTableDataInput *)realloc( where_fields,
						num_where_fields * sizeof(tTableDataInput) );

					where_fields[num_where_fields - 1].name = strdup(t2.tokens[i]);

					nextIsField = -1;
				}

				if ((strcmp(t2.tokens[i], "AND") == 0)
					|| (strcmp(t2.tokens[i], "OR") == 0))
					nextIsField = 1;
				else {
					if (nextIsField != -1) {
						if (strcmp(t2.tokens[i], "=") != 0) {
							char *tmp2 = NULL;
							int type;

							long idField = _idb_get_field_id(name,
								where_fields[num_where_fields - 1].name);
							tmp2 = _idb_get_input_value(trim(t2.tokens[i]), NULL);
							type = _idb_get_type_id_from_field(idField);

							if (type == IDB_TYPE_INT)
								where_fields[num_where_fields - 1].iValue = atoi(tmp2);
							else
							if (type == IDB_TYPE_LONG)
								where_fields[num_where_fields - 1].lValue = atol(tmp2);
							else
							if (type == IDB_TYPE_STR)
								where_fields[num_where_fields - 1].sValue = strdup(tmp2);

							free(tmp2); tmp2 = NULL;
						}
					}
					nextIsField = 0;
				}
			}
			free_tokens(t2);
		}

		ret = (idb_table_delete(name, num_where_fields, where_fields) >= 0) ? 0 : -ENOENT;
		free_tokens(t);
	}
	else
	if (strncmp(query, "DROP TABLE", 10) == 0) {
		char *name = NULL;

		t = tokenize(query, " ");
		name = strdup(t.tokens[2]);

		ret = idb_table_drop(name);
		free_tokens(t);
	}
	else
	if (strcmp(query, "SHOW TABLES") == 0) {
		idb_free_last_select_data();
		_last_tds = idb_tables_show();

		ret = (_last_tds.num_rows == 0) ? -ENOENT : 0;
	} 
	else
	if (strncmp(query, "SELECT", 6) == 0) {
		tTokenizer t2;
		int nextIsField;
		int num_fields = 0;
		int num_where_fields = 0;
		tTableDataInput *where_fields = NULL;
		char *name = NULL;
		char *fields = NULL;
		char **aFields = NULL;

		idb_free_last_select_data();

		t = tokenize(query, " ");
		fields = strdup(t.tokens[1]);
		name = strdup(t.tokens[3]);

		if (strstr(query, "WHERE ") != NULL) {
			char *tmp = strstr(query, "WHERE ") + 6;

			where_fields = (tTableDataInput *)malloc( sizeof(tTableDataInput) );
			memset(where_fields, 0, sizeof(tTableDataInput));
			num_where_fields = 0;

			t2 = tokenize(tmp, " ");
			nextIsField = 1;
			for (i = 0; i < t2.numTokens; i++) {
				if ((nextIsField == 1) || (i == 0)) {
					num_where_fields++;
					where_fields = (tTableDataInput *)realloc( where_fields,
						num_where_fields * sizeof(tTableDataInput) );

					where_fields[num_where_fields - 1].name = strdup(t2.tokens[i]);

					nextIsField = -1;
				}

				if ((strcmp(t2.tokens[i], "AND") == 0)
					|| (strcmp(t2.tokens[i], "OR") == 0))
					nextIsField = 1;
				else {
					if (nextIsField != -1) {
						if (strcmp(t2.tokens[i], "=") != 0) {
							char *tmp2 = NULL;
							int type;

							long idField = _idb_get_field_id(name,
								where_fields[num_where_fields - 1].name);
							tmp2 = _idb_get_input_value(trim(t2.tokens[i]), NULL);
							type = _idb_get_type_id_from_field(idField);

							if (type == IDB_TYPE_INT)
								where_fields[num_where_fields - 1].iValue = atoi(tmp2);
							else
							if (type == IDB_TYPE_LONG)
								where_fields[num_where_fields - 1].lValue = atol(tmp2);
							else
							if (type == IDB_TYPE_STR)
								where_fields[num_where_fields - 1].sValue = strdup(tmp2);

							free(tmp2); tmp2 = NULL;
						}
					}
					nextIsField = 0;
				}
			}
			free_tokens(t2);
		}

		t2 = tokenize(fields, ",");
		num_fields = t2.numTokens;
		aFields = (char **)malloc( num_fields * sizeof(char *) );
		for (i = 0; i < t2.numTokens; i++)
			aFields[i] = strdup(t2.tokens[i]);
		free_tokens(t2);

		_last_tds = idb_table_select(name, num_fields, aFields, num_where_fields, where_fields);
		ret = (_last_tds.num_rows == 0) ? -ENOENT : 0;
		free_tokens(t);

		/* We comment this out as it is for debugging purposes only */
		//idb_results_dump( _last_tds );

		free(aFields);
		free_tokens(t2);
	}
	else
	if (strcmp(query, "COMMIT") == 0)
		ret = idb_save(NULL);
	else
	if (strcmp(query, "ROLLBACK") == 0) {
		char *filename = strdup( idb_get_filename() );

		idb_free();
		idb_init();

		ret = idb_load(filename);
		free(filename);
		filename = NULL;
	}
	else
	if (strcmp(query, "CLOSE") == 0) {
		DPRINTF("%s: Closing database session\n", __FUNCTION__);
		idb_free();

		ret = 0;
	}
	else
	if (strcmp(query, "DUMP") == 0) {
		DPRINTF("\n");
		DPRINTF("********* Dumping data *********\n");
		DPRINTF("\n");
		DPRINTF("Filename: %s\n", idb_get_filename() );
		idb_dump();
		ret = 0;
	}
	else
		ret = -ENOTSUP;

	ts = idb_get_time( TIME_DIFF );
	myTime = ((ts.tv_sec * 1000000000) + ts.tv_nsec) / (float)1000;

	/* Log the query result */
	if (log) {
		int fd;
		char tmp[1024] = { 0 };

		snprintf(tmp, sizeof(tmp), "\" returned in %.3f ms, code %d\n",
			myTime, ret);
		fd = open(_idb_querylog, O_WRONLY | O_CREAT | O_APPEND, 0644);
		write(fd, tmp, strlen(tmp));
		close(fd);
	}

cleanup:
	if ((_perf_measure) && (tss.tv_sec > 0) && (tss.tv_nsec > 0)) {
		tse = utils_get_time( TIME_CURRENT );
		desc_printf(gIO, gFd, "PERF: Query \"%s\" was being processed for %.3f microseconds\n\n",
			query, get_time_float_us( tse, tss ));
	}

	DPRINTF("%s: Returning error code %d (%s), time is %.3f ms\n", __FUNCTION__, ret,
		strerror(-ret), myTime);
	return ret;
}

tTableDataSelect idb_get_last_select_data(void)
{
	return _last_tds;
}

void idb_free_last_select_data(void)
{
	idb_results_free( &_last_tds );
}

void idb_free(void)
{
	struct timespec ts;
	struct timespec tss;
	float myTime = -1;

	if (_idb_mincrypt_enabled == 1)
		wrap_mincrypt_cleanup();

	free(idb_tables);
	free(idb_fields);
	free(idb_tabdata);
	free(_idb_filename);
	_idb_filename = NULL;
	idb_tables = NULL;
	idb_fields = NULL;
	idb_tabdata = NULL;

	idb_tables_num = 0;
	idb_fields_num = 0;
	idb_tabdata_num = 0;

	if ((_idb_session_start.tv_nsec > 0) || (_idb_session_start.tv_sec > 0)) {
		tss = idb_get_time( TIME_CURRENT );
		timespecsub(&tss, &_idb_session_start, &ts);
	        myTime = ((ts.tv_sec * 1000000000) + ts.tv_nsec) / (float)1000;
	}

	/* Log the session time */
	if (_idb_querylog != NULL) {
		int fd;
		char tmp[4096] = { 0 };

		fd = open(_idb_querylog, O_WRONLY | O_CREAT | O_APPEND, 0644);
		write(fd, tmp, strlen(tmp));

		snprintf(tmp, sizeof(tmp), "Queries = { select: %d, insert: %d, update: %d, delete: %d, "
			"create: %d, drop: %d, total: %d }\n", _idb_num_queries_select, _idb_num_queries_insert,
			_idb_num_queries_update, _idb_num_queries_delete, _idb_num_queries_create,
			_idb_num_queries_drop, _idb_num_queries);
		write(fd, tmp, strlen(tmp));

		snprintf(tmp, sizeof(tmp), "Session finished in %.3f ms (%.3f s), %d queries processed\n",
			myTime, myTime / 1000., _idb_num_queries);

		write(fd, tmp, strlen(tmp));
		close(fd);
	}

	if (myTime > -1) {
		DPRINTF("%s: Queries = { select: %d, insert: %d, update: %d, delete: %d, create: %d, drop: %d, total: %d }\n",
			__FUNCTION__,
			_idb_num_queries_select, _idb_num_queries_insert, _idb_num_queries_update, _idb_num_queries_delete,
			_idb_num_queries_create, _idb_num_queries_drop, _idb_num_queries);

		DPRINTF("%s: Session finished in %.3f ms (%.3f s), %d queries processed\n", __FUNCTION__,
			myTime, myTime / 1000., _idb_num_queries);
	}

	_idb_num_queries = 0;
}

long _idb_get_free_id(int type)
{
	long i;
	long id = -1;

	if (type == IDB_TABLES) {
		for (i = 0; i < idb_tables_num; i++) {
			if (idb_tables[i].id > id)
				id = idb_tables[i].id;
		}
	}
	else
	if (type == IDB_FIELDS) {
		for (i = 0; i < idb_fields_num; i++) {
			if (idb_fields[i].id > id)
				id = idb_fields[i].id;
		}
	}
	else
	if (type == IDB_TABDATA) {
		for (i = 0; i < idb_tabdata_num; i++) {
			if (idb_tabdata[i].id > id)
				id = idb_tabdata[i].id;
		}
	}

	return id + 1;
}

char *_idb_get_table_name(long id)
{
	long i;

	for (i = 0; i < idb_tables_num; i++) {
		if (idb_tables[i].id == id)
			return strdup( idb_tables[i].name );
	}

	return NULL;
}

char *_idb_get_type(int type)
{
	if (type == IDB_TYPE_INT)
		return "int";
	else
	if (type == IDB_TYPE_LONG)
		return "long";
	else
	if (type == IDB_TYPE_STR)
		return "string";
	else
	if (type == IDB_TYPE_FILE)
		return "file";

	return "unknown";
}

int idb_type_id(char *type)
{
	int ret = 0;

	if (strcmp(type, "int") == 0)
		ret = IDB_TYPE_INT;
	else
	if (strcmp(type, "long") == 0)
		ret = IDB_TYPE_LONG;
	else
	if (strcmp(type, "string") == 0)
		ret = IDB_TYPE_STR;
	else
	if (strcmp(type, "file") == 0)
		ret = IDB_TYPE_FILE;

	return ret;
}

int _idb_get_table_field_count(long id)
{
	long i;
	int num = 0;

	if (id == -1)
		return -1;

	for (i = 0; i < idb_fields_num; i++) {
		if (idb_fields[i].idTable == id)
			num++;
	}

	return num;
}

long _idb_table_add(char *name, char *comment)
{
	long id;

	idb_tables = realloc(idb_tables, (idb_tables_num + 1) * sizeof(tTableDef));
	if (idb_tables == NULL)
		return -1;

	id = _idb_get_free_id( IDB_TABLES );
	if (id < 0)
		return -1;

	idb_tables[idb_tables_num].id = id;
	idb_tables[idb_tables_num].name = strdup(name);
	if (comment != NULL)
		idb_tables[idb_tables_num].comment = strdup(comment);
	idb_tables_num++;

	DPRINTF("%s: Table '%s' has been added successfully with%s\n", __FUNCTION__, name,
		(comment == NULL) ? "out any comment" : " a comment");

	return id;
}

long _idb_table_field_add(long idTable, char *name, int type)
{
	long id;

	idb_fields = realloc(idb_fields, (idb_fields_num + 1) * sizeof(tTableFieldDef));
	if (idb_fields == NULL)
		return -1;

	id = _idb_get_free_id( IDB_FIELDS );
	if (id < 0)
		return -1;

	idb_fields[idb_fields_num].id = id;
	idb_fields[idb_fields_num].idTable = idTable;
	idb_fields[idb_fields_num].name = strdup(name);
	idb_fields[idb_fields_num].type = type;
	idb_fields_num++;

	DPRINTF("%s: Field '%s' of type %s has been added successfully to table '%s'\n", __FUNCTION__, name,
		_idb_get_type(type), _idb_get_table_name(idTable));

	return id;
}

int _idb_get_type_id_from_field(long idField)
{
	long i;

	for (i = 0; i < idb_fields_num; i++) {
		if (idb_fields[i].id == idField)
			return idb_fields[i].type;
	}

	return -1;
}

long _idb_table_row_field_data_add(long idField, long idRow, tTableDataInput td)
{
	int fieldType;
	int skip = 0;
	long id;

	fieldType = _idb_get_type_id_from_field(idField);
	if (fieldType < 0) {
		DPRINTF("%s: Invalid field type 0x%02x\n", __FUNCTION__, fieldType);
		return -1;
	}

	idb_tabdata = realloc(idb_tabdata, (idb_tabdata_num + 1) * sizeof(tTableData));
	if (idb_tabdata == NULL) {
		DPRINTF("%s: Cannot reallocate data table\n", __FUNCTION__);
		return -1;
	}

	id = _idb_get_free_id( IDB_TABDATA );
	if (id < 0) {
		DPRINTF("%s: Invalid data table free id %ld\n", __FUNCTION__, id);
		return -1;
	}

	idb_tabdata[idb_tabdata_num].id = id;
	idb_tabdata[idb_tabdata_num].idField = idField;
	idb_tabdata[idb_tabdata_num].idRow = idRow;
	switch (fieldType) {
		case IDB_TYPE_INT:
					idb_tabdata[idb_tabdata_num].iValue = td.iValue;
					break;
		case IDB_TYPE_LONG:
					idb_tabdata[idb_tabdata_num].lValue = td.lValue;
					break;
		case IDB_TYPE_STR:
		case IDB_TYPE_FILE:
					if (td.sValue == NULL) {
						DPRINTF("%s: Invalid string value\n", __FUNCTION__);
						skip = 1;
						id = -1;
						break;
					}
					idb_tabdata[idb_tabdata_num].sValue = strdup( td.sValue );
					break;
		default:
					DPRINTF("%s: Invalid type\n", __FUNCTION__);
					skip = 1;
					id = -1;
					break;
	}

	if (!skip)
		idb_tabdata_num++;

	return id;
}

long _idb_table_row_field_file_add(long idField, long idRow, char *file)
{
	tTableDataInput td = { 0 };

	/* Allow saving this only for file type */
	if (_idb_get_type_id_from_field(idField) != IDB_TYPE_FILE)
		return -1;

	td.sValue = file;

	return _idb_table_row_field_data_add(idField, idRow, td);
}

long _idb_table_id_from_field(long idField)
{
	long i;

	for (i = 0; i < idb_fields_num; i++) {
		if (idb_fields[i].id == idField)
			return idb_fields[i].idTable;
	}

	return -1;
}

char *_idb_table_name(long idField)
{
        long i;

        for (i = 0; i < idb_tables_num; i++) {
		if (idb_tables[i].id == idField)
			return strdup( idb_tables[i].name );
	}

	return NULL;
}

long _idb_table_get_next_row(long idField)
{
	long i;
	long idRow = 0;

	for (i = 0; i < idb_tabdata_num; i++) {
		if ((idb_tabdata[i].idField == idField)
			&& (idb_tabdata[i].idRow > idRow))
			idRow = idb_tabdata[i].idRow;
	}

	return idRow + 1;
}

long _idb_table_row_field_add(long idField, long idRow, tTableDataInput td)
{
	if (_idb_get_type_id_from_field(idField) == IDB_TYPE_FILE) {
		char file[128] = { 0 };
		FILE *fp = NULL;

		if (_idb_datadir != NULL) {
			snprintf(file, sizeof(file), "%s/%s-%ld-%ld.cdb",
				_idb_datadir,
				_idb_table_name( _idb_table_id_from_field(idField) ),
				idField, idRow);

			fp = fopen(file, "wb");
			if (fp == NULL) {
				DPRINTF("%s: Cannot save '%s' file\n", __FUNCTION__, file);
				return -EPERM;
			}
			fwrite(td.cData, td.cData_len, 1, fp);
			fclose(fp);

			return _idb_table_row_field_file_add(idField, idRow, file);
		}
		else {
			DPRINTF("%s: Data directory not set, cannot save file\n", __FUNCTION__);
			return -EINVAL;
		}
	}

	return _idb_table_row_field_data_add(idField, idRow, td);
}

int _idb_table_row_field_delete(long idField, long idRow)
{
	long i, id = -1;
	long lid = -1;

	id = _idb_table_get_next_row(idField) - 1;
	if (id == -1)
		return -EINVAL;

	lid = idb_tabdata_num - 1;
	if (lid < 0)
		return -EINVAL;

	for (i = 0; i < idb_tabdata_num; i++) {
		if ((idb_tabdata[i].idField == idField)
			&& (idb_tabdata[i].idRow == idRow)) {
			id = i;
			break;
		}
	}

	if (id == -1)
		return -EINVAL;

	free(idb_tabdata[id].sValue);
	idb_tabdata[id].sValue = NULL;

	idb_tabdata[id].id = idb_tabdata[lid].id;
	idb_tabdata[id].idField = idb_tabdata[lid].idField;
	idb_tabdata[id].idRow = idb_tabdata[lid].idRow;
	idb_tabdata[id].iValue = idb_tabdata[lid].iValue;
	idb_tabdata[id].lValue = idb_tabdata[lid].lValue;
	idb_tabdata[id].sValue = idb_tabdata[lid].sValue;
	idb_tabdata_num--;

	idb_tabdata = realloc( idb_tabdata, idb_tabdata_num * sizeof(tTableData) );
	if (idb_tabdata == NULL)
		return -ENOMEM;

	return 0;
}

int _idb_table_field_delete(long idField)
{
	long i, id = -1;
	long lid = -1;

	lid = idb_fields_num - 1;
	if (lid < 0)
		return -EINVAL;

	for (i = 0; i < idb_fields_num; i++) {
		if (idb_fields[i].id == idField) {
			id = i;
			break;
		}
	}

	if (id == -1)
		return -EINVAL;

	free(idb_fields[id].name);
	idb_fields[id].name = NULL;

	idb_fields[id].id = idb_fields[lid].id;
	idb_fields[id].type = idb_fields[lid].type;
	idb_fields[id].name = idb_fields[lid].name;
	idb_fields[id].idTable = idb_fields[lid].idTable;
	idb_fields_num--;

	idb_fields = realloc( idb_fields, idb_fields_num * sizeof(tTableFieldDef) );
	if (idb_fields == NULL)
		return -ENOMEM;

	return 0;
}

int _idb_table_delete(long idTable)
{
	long i, id = -1;
	long lid = -1;
	int comment = 0;

	lid = idb_tables_num - 1;
	if (lid < 0)
		return -EINVAL;

	for (i = 0; i < idb_tables_num; i++) {
		if (idb_tables[i].id == idTable) {
			id = i;
			break;
		}
	}

	if (id == -1)
		return -EINVAL;

	if (idb_tables[lid].comment != NULL) {
		free(idb_tables[id].comment);
		idb_tables[id].comment = NULL;
		comment = 1;
	}
	free(idb_tables[id].name);
	idb_tables[id].name = NULL;

	idb_tables[id].id = idb_tables[lid].id;
	idb_tables[id].name = idb_tables[lid].name;
	if (comment)
		idb_tables[id].comment = idb_tables[lid].comment;
	idb_tables_num--;

	idb_tables = realloc( idb_tables, idb_tables_num * sizeof(tTableDef) );
	if (idb_tables == NULL)
		return -ENOMEM;

	return 0;
}

long _idb_table_id(char *name)
{
	long i;

	for (i = 0; i < idb_tables_num; i++) {
		if (strcmp(idb_tables[i].name, name) == 0)
			return idb_tables[i].id;
	}

	return -1;
}

long _idb_table_get_id_from_field_id(long idField)
{
	long i;

	for (i = 0; i < idb_fields_num; i++) {
		if (idb_fields[i].id == idField)
			return idb_fields[i].idTable;
	}

	return -1;
}

long _idb_get_field_id(char *table, char *field)
{
	long i;
	long idTable = -1;

	for (i = 0; i < idb_tables_num; i++) {
		if (strcmp(idb_tables[i].name, table) == 0)
			idTable = idb_tables[i].id;
	}

	if (idTable < 0)
		return -1;

	for (i = 0; i < idb_fields_num; i++) {
		if ((strcmp(idb_fields[i].name, field) == 0)
			&& (idb_fields[i].idTable == idTable))
			return idb_fields[i].id;
	}

	return -1;
}

int idb_table_create(char *name, int num_fields, tTableFieldDef *fields, char *comment)
{
	long i;
	long idTable = -1;

	if (num_fields <= 0) {
		DPRINTF("%s: Num fields is 0 or less, cannot continue\n", __FUNCTION__);
		return -EINVAL;
	}

	if (_idb_table_id(name) >= 0) {
		DPRINTF("%s: Table '%s' already exists\n", __FUNCTION__, name);
		return -EEXIST;
	}

	if ((idTable = _idb_table_add(name, comment)) < 0) {
		DPRINTF("%s: Cannot add table '%s'\n", __FUNCTION__, name);
		return -EIO;
	}

	for (i = 0; i < num_fields; i++) {
		_idb_table_field_add(idTable, fields[i].name, fields[i].type);
	}

	_idb_num_queries++;
	_idb_num_queries_create++;

	DPRINTF("%s: Table '%s' with %d fields added successfully\n", __FUNCTION__, name, num_fields);
	return 0;
}

int idb_table_insert(char *table_name, int num_data, tTableDataInput *td)
{
	int i, num = 0;
	long idRow = -1;
	long idField = -1;

	for (i = 0; i < num_data; i++) {
		idField = _idb_get_field_id(table_name, td[i].name);
		if (idField >= 0) {
			if (idRow == -1)
				idRow = _idb_table_get_next_row(idField);

			/* minCrypt encryption is currently supported only for string values */
			if ((_idb_mincrypt_enabled == 1)
				&& (_idb_get_type_id_from_field(idField) == IDB_TYPE_STR)) {
				size_t nl;
				char *tmp = (char *)wrap_mincrypt_encrypt( (unsigned char *)td[i].sValue,
						strlen(td[i].sValue), idRow, &nl);

				if (tmp != NULL) {
					char *tmp2 = (char *)wrap_mincrypt_base64_encode((unsigned char *)tmp,
							&nl);

					if (tmp2 != NULL) {
						free(td[i].sValue);

						/* Substitute string value to base64-encoded encrypted value */
						td[i].sValue = strdup( tmp2 );
						free(tmp2);
					}
					free(tmp);
				}
			}

			DPRINTF("%s: Inserting new field '%s' to '%s', idField = %ld, idRow = %ld\n", __FUNCTION__,
				td[i].name, table_name, idField, idRow);
			if (_idb_table_row_field_add(idField, idRow, td[i]) >= 0)
				num++;
		}
	}

	_idb_num_queries++;
	_idb_num_queries_insert++;

	if (num != num_data) {
		DPRINTF("%s: Number of fields added is %d, number of fields that *should* have been added is %d\n",
			__FUNCTION__, num, num_data);
		return -EIO;
	}

	return 0;
}

int _idb_row_fields_match(char *table, long idRow, int num_fields, tTableDataInput *fields)
{
	int i;
	int ret = 0;
	long j;
	long idField;

	for (j = 0; j < idb_tabdata_num; j++) {
		if (idb_tabdata[j].idRow == idRow) {
			for (i = 0; i < num_fields; i++) {
				idField = _idb_get_field_id(table, fields[i].name);
				if ((idField >= 0) && (idb_tabdata[j].idField == idField)) {
					switch (_idb_get_type_id_from_field(idField)) {
						case IDB_TYPE_INT:	ret += (fields[i].iValue == idb_tabdata[j].iValue);
									break;
						case IDB_TYPE_LONG:	ret += (fields[i].lValue == idb_tabdata[j].lValue);
									break;
						case IDB_TYPE_STR:	if (_idb_mincrypt_enabled == 0) {
										ret += (strcmp(fields[i].sValue, idb_tabdata[j].sValue) == 0);
									}
									else {
										char *tmp = NULL;
										unsigned char *tmp2 = NULL;
										size_t len = strlen(idb_tabdata[j].sValue);
										int rsize = -1;

										tmp = (char *)wrap_mincrypt_base64_decode((unsigned char *)
											idb_tabdata[j].sValue, &len);

										if (tmp != NULL) {
											tmp2 = wrap_mincrypt_decrypt((unsigned char *)tmp, len,
												idb_tabdata[j].idRow,
												&len, &rsize);
											if (tmp2 != NULL) {
												tmp2[len] = 0;
												ret += (strcmp(fields[i].sValue,
														(char *)tmp2) == 0);
												free(tmp2);
											}
										}
										free(tmp);
									}
									break;
						case IDB_TYPE_FILE:	ret += 1;
									DPRINTF("%s: File conditions are not supported!\n", __FUNCTION__);
									break;
						default:		DPRINTF("%s: Unknown data type\n", __FUNCTION__);
									break;
					}
				}
			}
		}
	}

	return (ret == num_fields);
}

int idb_table_update_row(char *table_name, long idRow, int num_data, tTableDataInput *td)
{
	int i;
	long idField = -1;

	for (i = 0; i < num_data; i++) {
		idField = _idb_get_field_id(table_name, td[i].name);
		if (idField >= 0) {
			 _idb_table_row_field_delete(idField, idRow);
			_idb_table_row_field_add(idField, idRow, td[i]);
		}
	}

	_idb_num_queries++;
	_idb_num_queries_update++;
	return 0;
}

int idb_table_update(char *table_name, int num_fields, tTableDataInput *td, int num_where_fields, tTableDataInput *where_fields)
{
	long i, j;
	long idRow;
	long row_cnt;
	long idTable;
	long idField;

	idTable = _idb_table_id(table_name);
	if (idTable < 0)
		return -ENOENT;

	DPRINTF("%s: Number of fields is %d\n", __FUNCTION__, num_fields);
	row_cnt = 0;
	for (i = 0; i < num_fields; i++) {
		idField = _idb_get_field_id(table_name, td[i].name);
		if (idField < 0) {
			DPRINTF("%s: Cannot find field '%s'\n", __FUNCTION__, td[i].name);
			return -EINVAL;
		}

		for (j = 0; j < idb_tabdata_num; j++) {
			if (idb_tabdata[j].idField == idField) {
				if (!_idb_row_fields_match(table_name, idb_tabdata[j].idRow, num_where_fields, where_fields))
					continue;
				FILE *fp = NULL;
				row_cnt++;
				idRow = idb_tabdata[j].idRow;

				DPRINTF("%s: Found matching field with internal ID %ld\n", __FUNCTION__, idb_tabdata[j].id);
				switch (_idb_get_type_id_from_field(idField)) {
					case IDB_TYPE_INT:
								idb_tabdata[j].iValue = td[i].iValue;
								break;
					case IDB_TYPE_LONG:
								idb_tabdata[j].lValue = td[i].lValue;
								break;
					case IDB_TYPE_STR:
								free(idb_tabdata[j].sValue);
								idb_tabdata[j].sValue = NULL;
								if (_idb_mincrypt_enabled == 1) {
									size_t nl;
									unsigned char *tmp = wrap_mincrypt_encrypt(
										(unsigned char *)td[i].sValue, strlen(td[i].sValue),
										idRow, &nl);

									if (tmp != NULL) {
										char *tmp2 = (char *)
											wrap_mincrypt_base64_encode(tmp, &nl);

										if (tmp2 != NULL) {
											free(td[i].sValue);
											td[i].sValue = strdup( tmp2 );
											free(tmp2);
										}
										free(tmp);
									}
								}
								idb_tabdata[j].sValue = strdup( td[i].sValue );
								break;
					case IDB_TYPE_FILE:
								fp = fopen(idb_tabdata[j].sValue, "w");
								fwrite(td[i].cData, td[i].cData_len, 1, fp);
								fclose(fp);
								break;
					default:
								DPRINTF("%s: Unsupported data type\n", __FUNCTION__);
								break;
				}
			}
		}
	}

	_idb_num_queries++;
	_idb_num_queries_update++;
	return row_cnt;
}

int idb_table_delete_row(char *table_name, long idRow)
{
	int affected;
	long i, tId = -1;
	long row_cnt;

	tId = _idb_table_id(table_name);
	if (tId == -1)
		return -EINVAL;

	affected = 0;
iter:
	row_cnt = 0;
	for (i = 0; i < idb_tabdata_num; i++) {
		if (( _idb_table_get_id_from_field_id(idb_tabdata[i].idField) == tId)
			&& (idb_tabdata[i].idRow == idRow))
			if (_idb_table_row_field_delete(idb_tabdata[i].idField, idRow) == 0) {
				affected++;
				row_cnt++;
			}
	}

	if (row_cnt > 0)
		goto iter;

	_idb_num_queries++;
	_idb_num_queries_delete++;
	DPRINTF("%s: Number of affected items is %d\n", __FUNCTION__, affected);
	return affected;
}

int idb_table_delete(char *table_name, int num_where_fields, tTableDataInput *where_fields)
{
	int row_cnt;
	int affected;
	long i;
	long idTable;
	long idRow;

	idTable = _idb_table_id(table_name);
	if (idTable < 0)
		return -ENOENT;

	affected = 0;
iter:
	row_cnt = 0;
	for (i = 0; i < idb_tabdata_num; i++) {
		if (_idb_table_get_id_from_field_id(idb_tabdata[i].idField) == idTable) {
			if (!_idb_row_fields_match(table_name, idb_tabdata[i].idRow, num_where_fields, where_fields))
				continue;
			idRow = idb_tabdata[i].idRow;

			row_cnt++;
			affected++;

			DPRINTF("%s: Found matching field with idRow %ld (internal ID %ld)\n", __FUNCTION__,
					idRow, idb_tabdata[i].id);
			idb_table_delete_row(table_name, idRow);
		}
	}

	if (row_cnt > 0)
		goto iter;

	_idb_num_queries++;
	_idb_num_queries_delete++;
	DPRINTF("%s: Number of affected items is %d\n", __FUNCTION__, affected);
	return row_cnt;
}

int idb_table_drop(char *table_name)
{
	int ret;
	long i;
	long idTable;

	idTable = _idb_table_id(table_name);
	if (idTable < 0)
		return -ENOENT;

	ret = idb_table_delete(table_name, 0, NULL);
	if (ret != 0)
		return -ENOENT;

	for (i = 0; i < idb_fields_num; i++) {
		if (idb_fields[i].idTable == idTable)
			_idb_table_field_delete(idb_fields[i].id);
	}

	for (i = 0; i < idb_tables_num; i++) {
		if (idb_tables[i].id == idTable)
			_idb_table_delete(idb_tables[i].id);
	}

	_idb_num_queries++;
	_idb_num_queries_drop++;
	return 0;
}

int _idb_tables_dump(void)
{
	long i;

	for (i = 0; i < idb_tables_num; i++) {
		dump_printf("Table #%ld:\n", i + 1);
		dump_printf("\tInternal ID: %ld\n", idb_tables[i].id);
		dump_printf("\tName: %s\n", idb_tables[i].name);
		dump_printf("\tNumber of fields: %d\n", _idb_get_table_field_count(idb_tables[i].id));
		dump_printf("\tComment: %s\n", idb_tables[i].comment ? idb_tables[i].comment : "<null>");
	}

	return 0;
}

int _idb_fields_dump(void)
{
	long i;

	for (i = 0; i < idb_fields_num; i++) {
		dump_printf("Table field #%ld:\n", i + 1);
		dump_printf("\tInternal ID: %ld\n", idb_fields[i].id);
		dump_printf("\tFor table: %s (ID = %ld)\n", _idb_get_table_name(idb_fields[i].idTable),
			idb_fields[i].idTable);
		dump_printf("\tName: %s\n", idb_fields[i].name);
		dump_printf("\tType: %s (code %d)\n", _idb_get_type(idb_fields[i].type),
			idb_fields[i].type);
	}

	return 0;
}

int _idb_tabledata_dump(void)
{
	long i;
	int fieldType;

	for (i = 0; i < idb_tabdata_num; i++) {
		fieldType = _idb_get_type_id_from_field(idb_tabdata[i].idField);

		dump_printf("Table data #%ld:\n", i + 1);
		dump_printf("\tInternal ID: %ld\n", idb_tabdata[i].id);
		dump_printf("\tInternal field ID: %ld (%s)\n", idb_tabdata[i].idField,
			_idb_get_type(fieldType));
		dump_printf("\tRow number: %ld\n", idb_tabdata[i].idRow);

		if (fieldType == IDB_TYPE_INT)
			dump_printf("\tInteger value: %d\n", idb_tabdata[i].iValue);
		else
		if (fieldType == IDB_TYPE_LONG)
			dump_printf("\tLong value: %ld\n", idb_tabdata[i].lValue);
		else
		if (fieldType == IDB_TYPE_STR) {
			char *data = NULL;
			/* Provide valid data dump even if using encryption */
			if (_idb_mincrypt_enabled == 1) {
				char *tmp2 = NULL;
				unsigned char *tmp = NULL;
				size_t len = strlen(idb_tabdata[i].sValue);
				int rsize = -1;

				tmp = wrap_mincrypt_base64_decode((unsigned char *)
					idb_tabdata[i].sValue, &len);

				if (tmp != NULL) {
					tmp2 = (char *)wrap_mincrypt_decrypt(tmp, len,
						idb_tabdata[i].idRow,
						&len, &rsize);
					if (tmp2 != NULL) {
						tmp2[len] = 0;
						data = strdup( tmp2 );
						free(tmp2);
					}
				}
				free(tmp);
			}
			else
				data = strdup( idb_tabdata[i].sValue );
			dump_printf("\tString value: %s%s\n", data,
				(_idb_mincrypt_enabled == 1) ? " (showing decrypted value)" : "");
			free(data);
		}
		else
		if (fieldType == IDB_TYPE_FILE)
			dump_printf("\tFile value: %s (%ld bytes)\n", idb_tabdata[i].sValue,
				get_file_size(idb_tabdata[i].sValue));
	}

	return 0;
}

void idb_dump(void)
{
	dump_printf("==================================\n");
	dump_printf("Dumping data for internal database\n");
	dump_printf("==================================\n");
	dump_printf("Metrics:\n");
	dump_printf("\tTables: %d\n", idb_tables_num);
	dump_printf("\tFields: %d\n", idb_fields_num);
	dump_printf("\tData: %d\n", idb_tabdata_num);

	_idb_tables_dump();
	_idb_fields_dump();
	_idb_tabledata_dump();
}

int _idb_fill_cdata(tTableDataField *tdi, char *filename)
{
	int fd;
	long len = 0;
	char buf[1024] = { 0 };
	long total = 0;

	len = get_file_size( filename );
	DPRINTF("%s: Opening file '%s', size %ld\n", __FUNCTION__, filename, len);

	tdi->cData = (void *)malloc( len * sizeof(void *) );
	if (tdi->cData == NULL) {
		DPRINTF("%s: Cannot allocate memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	memset(tdi->cData, 0, len);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		free(tdi->cData);
		DPRINTF("%s: Cannot open %s for reading\n", __FUNCTION__, filename);
		return -ENOENT;
	}

	DPRINTF("%s: File %s opened for reading\n", __FUNCTION__, filename);

	while ((len = read(fd, buf, sizeof(buf))) > 0) {
		memcpy(tdi->cData + total, buf, len);

		total += len;
	}

	tdi->cData_len = total;
	close(fd);

	DPRINTF("%s: cData filled by %ld byte(s)\n", __FUNCTION__,
			tdi->cData_len);

	return 0;
}

tTableDataSelect idb_tables_show(void)
{
	long i;
	tTableDataSelect tds;

	tds.rows = NULL;
	tds.num_rows = 0;
	tds.rows = (tTableDataRow *)malloc( idb_tables_num * sizeof(tTableDataRow) );
	for (i = 0; i < idb_tables_num; i++) {
		tds.rows[i].num_fields = 1;
		tds.rows[i].tdi = (tTableDataField *)malloc( sizeof(tTableDataField) );
		tds.rows[i].tdi[0].name = strdup( "Name" );
		tds.rows[i].tdi[0].type = IDB_TYPE_STR;
		tds.rows[i].tdi[0].iValue = 0;
		tds.rows[i].tdi[0].lValue = 0;
		tds.rows[i].tdi[0].sValue = strdup( idb_tables[i].name );
		tds.rows[i].tdi[0].cData = NULL;
		tds.rows[i].tdi[0].cData_len = 0;
	}

	_idb_num_queries++;
	_idb_num_queries_select++;
	tds.num_rows = idb_tables_num;
	return tds;
}

tTableDataSelect idb_table_select(char *table, int num_fields, char **fields, int num_where_fields, tTableDataInput *where_fields)
{
	long i, j, l;
	long row_cnt;
	long idTable;
	long idField;
	long idRow = 0;
	long row_idx = 0;
	long *rows_present = NULL;
	tTableDataSelect tds;

	idTable = _idb_table_id(table);
	if (idTable < 0)
		return tdsNone;

	/* Support asterisk only as a single field component */
	if ((num_fields == 1) && (strcmp(fields[0], "*") == 0)) {
		long new;
		new = l = 0;
		for (j = 0; j < idb_fields_num; j++) {
			if (idb_fields[j].idTable == idTable)
				l++;
		}

		new = (num_fields + (int)(l - 1));
		DPRINTF("%s: Asterisk found, array will be extended by %ld elements, new size is %ld\n", __FUNCTION__,
			l - 1, new);

		fields = realloc( fields, new * sizeof(char *) );

		l = 0;
		for (j = 0; j < idb_fields_num; j++) {
			if (idb_fields[j].idTable == idTable) {
				new = l;
				if ((idb_fields[j].name != NULL) && (strlen(idb_fields[j].name) > 0)) {
					fields[new] = strdup( idb_fields[j].name );
					l++;
				}
			}
		}

		num_fields += l - 1;
	}

	rows_present = (long *)malloc( sizeof(long) );

	tds.rows = NULL;
	tds.num_rows = row_cnt = 0;
	for (i = 0; i < num_fields; i++) {
		DPRINTF("%s: Accessing field '%s'\n", __FUNCTION__, fields[i]);

		idField = _idb_get_field_id(table, fields[i]);
		if (idField < 0) {
			free(rows_present);
			DPRINTF("%s: Cannot find field '%s'\n", __FUNCTION__, fields[i]);
			return tdsNone;
		}

		for (j = 0; j < idb_tabdata_num; j++) {
			if (idb_tabdata[j].idField == idField) {
				idRow = -1;
				for (l = 0; l < row_cnt; l++) {
					if (rows_present[l] == idb_tabdata[j].idRow) {
						idRow = idb_tabdata[j].idRow;
						row_idx = l;
						break;
					}
				}

				if (idRow < 0) {
					if (!_idb_row_fields_match(table, idb_tabdata[j].idRow, num_where_fields, where_fields))
						continue;
					rows_present = (long *)realloc(rows_present, (row_cnt + 1) * sizeof(long));
					idRow = idb_tabdata[j].idRow;

					rows_present[row_cnt] = idRow;
					row_idx = row_cnt;
					row_cnt++;

					if (tds.rows == NULL)
						tds.rows = (tTableDataRow *)malloc( sizeof(tTableDataRow) );
					else
						tds.rows = (tTableDataRow *)realloc(tds.rows, row_cnt * sizeof(tTableDataRow));

					tds.rows[row_idx].num_fields = 0;
					tds.rows[row_idx].tdi = (tTableDataField *)malloc( sizeof(tTableDataField) );

					DPRINTF("%s: New row detected, row count is %ld\n", __FUNCTION__, row_cnt);
				}

				tds.rows[row_idx].num_fields++;
				DPRINTF("%s: Setting up number of fields to %ld for row_idx %ld\n", __FUNCTION__,
					TDS_ROW(row_idx).num_fields, row_idx);
				tds.rows[row_idx].tdi = (tTableDataField *)realloc( tds.rows[row_idx].tdi,
						tds.rows[row_idx].num_fields * sizeof(tTableDataField));

				TDS_LAST_ROW(row_idx).name = strdup( fields[i] );
				TDS_LAST_ROW(row_idx).type =  _idb_get_type_id_from_field(
								idb_tabdata[j].idField );

				TDS_LAST_ROW(row_idx).iValue = 0;
				TDS_LAST_ROW(row_idx).lValue = 0;
				TDS_LAST_ROW(row_idx).sValue = 0;
				TDS_LAST_ROW(row_idx).cData  = NULL;
				TDS_LAST_ROW(row_idx).cData_len = 0;

				switch (TDS_LAST_ROW(row_idx).type) {
					case IDB_TYPE_INT:
						TDS_LAST_ROW(row_idx).iValue = idb_tabdata[j].iValue;
						DPRINTF("%s: Setting up integer value to result set\n",
							__FUNCTION__);
						break;
					case IDB_TYPE_LONG:
						TDS_LAST_ROW(row_idx).lValue = idb_tabdata[j].lValue;
						DPRINTF("%s: Setting up long value to result set\n",
							__FUNCTION__);
						break;
					case IDB_TYPE_STR:
						if (_idb_mincrypt_enabled == 1) {
							unsigned char *tmp2 = NULL;
							char *tmp = NULL;
							size_t len = strlen(idb_tabdata[j].sValue);
							int rsize = -1;

							tmp = (char *)wrap_mincrypt_base64_decode((unsigned char *)
								idb_tabdata[j].sValue, &len);
							if (tmp != NULL) {
								tmp2 = wrap_mincrypt_decrypt((unsigned char *)tmp, len,
									idb_tabdata[j].idRow,
									&len, &rsize);

								if (tmp2 != NULL) {
									tmp2[len] = 0;

									free(idb_tabdata[j].sValue);
									idb_tabdata[j].sValue = strdup(
										(char *)tmp2 );

									free(tmp2);
								}
								free(tmp);
							}
						}
						TDS_LAST_ROW(row_idx).sValue = strdup(
								idb_tabdata[j].sValue);
						DPRINTF("%s: Setting up string value to result set\n",
							__FUNCTION__);
						break;
					case IDB_TYPE_FILE:
						DPRINTF("%s: Setting up cData value to result set\n",
							__FUNCTION__);
						_idb_fill_cdata(&TDS_LAST_ROW(row_idx), idb_tabdata[j].sValue);
						break;
					default:
						DPRINTF("%s: Unhandled data type 0x%02x\n", __FUNCTION__,
							TDS_LAST_ROW(row_idx).type);
				}
			}
		}
	}

	_idb_num_queries++;
	_idb_num_queries_select++;
	tds.num_rows = row_cnt;
	return tds;
}

void idb_results_dump(tTableDataSelect tds)
{
	long i = 0, j = 0;

	if (tds.num_rows <= 0)
		return;

	dump_printf("=========================\n");
	dump_printf("Dumping select result set\n");
	dump_printf("=========================\n");
	dump_printf("Number of rows: %ld\n", tds.num_rows);

	for (i = 0; i < tds.num_rows; i++) {
		dump_printf("Row #%ld:\n", i + 1);
		dump_printf("\tNumber of fields: %ld\n", tds.rows[i].num_fields);

		for (j = 0; j < tds.rows[i].num_fields; j++) {
			dump_printf("\tField '%s'\n",
				tds.rows[i].tdi[j].name);
			dump_printf("\t\tType: %s (%d)\n",
				_idb_get_type(tds.rows[i].tdi[j].type),
				tds.rows[i].tdi[j].type);

			switch (tds.rows[i].tdi[j].type) {
				case IDB_TYPE_INT:
					dump_printf("\t\tInteger value: %d\n",
						tds.rows[i].tdi[j].iValue);
					break;
				case IDB_TYPE_LONG:
					dump_printf("\t\tLong value: %ld\n",
						tds.rows[i].tdi[j].lValue);
					break;
				case IDB_TYPE_STR:
					dump_printf("\t\tString value: %s\n",
						tds.rows[i].tdi[j].sValue);
					break;
				case IDB_TYPE_FILE:
					dump_printf("\t\tFile content length: %ld\n",
						tds.rows[i].tdi[j].cData_len);
					break;
				default:
					dump_printf("\t\tError: Invalid data type!\n");
					break;
			}
		}
	}

	dump_printf("======================\n");
	dump_printf("End of result set dump\n");
	dump_printf("======================\n");
}


void idb_results_show(BIO *io, int cfd, tTableDataSelect tds)
{
	long i = 0, j = 0;

	if (tds.num_rows <= 0) {
		DPRINTF("%s: No data in the data set (%ld rows)\n", __FUNCTION__, tds.num_rows);
		return;
	}

	for (i = 0; i < tds.rows[0].num_fields; i++) {
		desc_printf(io, cfd, "+");
		for (j = 0; j < 23; j++)
			desc_printf(io, cfd, "-");
	}
	desc_printf(io, cfd, "+\n| ");

	for (i = 0; i < tds.rows[0].num_fields; i++)
		desc_printf(io, cfd, " %20s | ", tds.rows[0].tdi[i].name);
	desc_printf(io, cfd, "\n");

	for (i = 0; i < tds.rows[0].num_fields; i++) {
		desc_printf(io, cfd, "+");
		for (j = 0; j < 23; j++)
			desc_printf(io, cfd, "-");
	}
	desc_printf(io, cfd, "+\n| ");

	for (i = 0; i < tds.num_rows; i++) {
		for (j = 0; j < tds.rows[i].num_fields; j++) {
			switch (tds.rows[i].tdi[j].type) {
				case IDB_TYPE_INT:
					desc_printf(io, cfd, "%21d | ",
						tds.rows[i].tdi[j].iValue);
					break;
				case IDB_TYPE_LONG:
					desc_printf(io, cfd, "%21ld | ",
						tds.rows[i].tdi[j].lValue);
					break;
				case IDB_TYPE_STR:
					desc_printf(io, cfd, "%21s | ",
						tds.rows[i].tdi[j].sValue);
					break;
				case IDB_TYPE_FILE:
					desc_printf(io, cfd, "<%21ld bytes> | ",
						tds.rows[i].tdi[j].cData_len);
					break;
				default:
					desc_printf(io, cfd, "%21s | ", "UNKNOWN FIELD TYPE");
					break;
			}
		}
		desc_printf(io, cfd, "\n");

		if (i < tds.num_rows - 1)
			desc_printf(io, cfd, "| ");
	}

	for (i = 0; i < tds.rows[0].num_fields; i++) {
		desc_printf(io, cfd, "+");
		for (j = 0; j < 23; j++)
			desc_printf(io, cfd, "-");
	}
	desc_printf(io, cfd, "+\n");
}

void idb_results_free(tTableDataSelect *tds)
{
	long i = 0, j = 0;

	if (tds->num_rows <= 0)
		return;

	for (i = 0; i < tds->num_rows; i++) {
		for (j = 0; j < tds->rows[i].num_fields; j++) {
			tds->rows[i].tdi[j].iValue = 0;
			tds->rows[i].tdi[j].lValue = 0;
			free(tds->rows[i].tdi[j].sValue);
			tds->rows[i].tdi[j].sValue = NULL;
			free(tds->rows[i].tdi[j].cData);
			tds->rows[i].tdi[j].cData = NULL;
			tds->rows[i].tdi[j].cData_len = 0;
		}

		tds->rows[i].num_fields = -1;
		free(tds->rows[i].tdi);
	}
	tds->num_rows = 0;

	free(tds->rows);
}

long _idb_save_header(int fd)
{
	long i, j, data_len = 0;
	char tmp[4] = { 0 };
	int ret;

	DPRINTF("%s: Saving header information\n", __FUNCTION__);

	/* CDVDB is the IDB header */
	if ((ret = data_write(fd, "CDVDB", 5, &data_len)) < 0)
		return ret;

	/* Write the header size placeholder */
	if ((ret = data_write(fd, tmp, 3, &data_len)) < 0)
		return ret;

	DPRINTF("%s: Number of tables is %d\n", __FUNCTION__, idb_tables_num);

	/* Set number of tables */
	WORDSTR(tmp, idb_tables_num);
	if ((ret = data_write(fd, tmp, 2, &data_len)) < 0)
		return ret;

	for (i = 0; i < idb_tables_num; i++) {
		/* Save internal ID */
		UINT32STR(tmp, idb_tables[i].id);
		if ((ret = data_write(fd, tmp, 4, &data_len)) < 0)
			return ret;

		/* Set length of table name */
		tmp[0] = strlen(idb_tables[i].name);
		if ((ret = data_write(fd, tmp, 1, &data_len)) < 0)
			return ret;
		/* Set table name */
		if ((ret = data_write(fd, idb_tables[i].name, tmp[0], &data_len)) < 0)
			return ret;

		DPRINTF("%s: Saving table '%s' with internal ID %ld\n", __FUNCTION__, idb_tables[i].name,
			idb_tables[i].id);

		/* Save comment only when comment is not NULL */
		if (idb_tables[i].comment != NULL) {
			/* Set length of comment field */
			tmp[0] = strlen(idb_tables[i].comment);
			if ((ret = data_write(fd, tmp, 1, &data_len)) < 0)
				return ret;
			DPRINTF("%s: Saving comment '%s' (%d bytes) for table '%s'\n", __FUNCTION__,
				idb_tables[i].comment, tmp[0], idb_tables[i].name);
			/* Set table comment */
			if ((ret = data_write(fd, idb_tables[i].comment, tmp[0], &data_len)) < 0)
				return ret;
		}
		else {
			/* Set zero-length comment field */
			tmp[0] = 0;
			if ((ret = data_write(fd, tmp, 1, &data_len)) < 0)
				return ret;

			DPRINTF("%s: No comment present for table '%s'\n", __FUNCTION__, idb_tables[i].name);
		}
		/* Set number of fields */
		tmp[0] = _idb_get_table_field_count(idb_tables[i].id);
		if ((ret = data_write(fd, tmp, 1, &data_len)) < 0)
			return ret;

		DPRINTF("%s: Table '%s' has %d field(s)\n", __FUNCTION__, idb_tables[i].name, tmp[0]);

		/* Save all fields for this table */
		for (j = 0; j < idb_fields_num; j++) {
			if (idb_fields[j].idTable == idb_tables[i].id) {
				/* Save internal ID */
				UINT32STR(tmp, idb_fields[j].id);
				if ((ret = data_write(fd, tmp, 4, &data_len)) < 0)
					return ret;
				tmp[0] = idb_fields[j].type;
				if ((ret = data_write(fd, tmp, 1, &data_len)) < 0)
					return ret;
				tmp[0] = strlen(idb_fields[j].name);
				if ((ret = data_write(fd, tmp, 1, &data_len)) < 0)
					return ret;
				tmp[0] = strlen(idb_fields[j].name);
				if ((ret = data_write(fd, idb_fields[j].name, tmp[0], &data_len)) < 0)
					return ret;

				DPRINTF("%s: Saving %s.%s, type %d (%s)\n", __FUNCTION__, idb_tables[i].name,
					idb_fields[j].name, idb_fields[j].type, _idb_get_type(idb_fields[j].type));
			}
		}
	}

	lseek(fd, 5, SEEK_SET);

	UINT32STR(tmp, (data_len - 5 - 3) );
	write(fd, tmp, 3);

	lseek(fd, data_len, SEEK_SET);

	DPRINTF("%s: Header size is %ld\n", __FUNCTION__, data_len);
	return data_len;
}

long _idb_save_data(int fd, long orig_data_len)
{
	long i, ret, data_len = 0;
	int fieldType;
	char tmp[4] = { 0 };

	DPRINTF("%s: Saving data\n", __FUNCTION__);

	/* Write the data size placeholder */
	if ((ret = data_write(fd, tmp, 3, &data_len)) < 0)
		return ret;

	/* Write the number of data entries */
	UINT32STR(tmp, idb_tabdata_num);
	if ((ret = data_write(fd, tmp, 3, &data_len)) < 0)
		return ret;

	for (i = 0; i < idb_tabdata_num; i++) {
		/* Get field type from field ID */
		fieldType = _idb_get_type_id_from_field(idb_tabdata[i].idField);

		/* Set field type */
		tmp[0] = fieldType;
		if ((ret = data_write(fd, tmp, 1, &data_len)) < 0)
			return ret;

		/* Save internal ID */
		UINT32STR(tmp, idb_tabdata[i].id);
		if ((ret = data_write(fd, tmp, 4, &data_len)) < 0)
			return ret;

		/* Save field ID */
		UINT32STR(tmp, idb_tabdata[i].idField);
		if ((ret = data_write(fd, tmp, 4, &data_len)) < 0)
			return ret;

		/* Save row ID */
		UINT32STR(tmp, idb_tabdata[i].idRow);
		if ((ret = data_write(fd, tmp, 4, &data_len)) < 0)
			return ret;

		/* Save field/row data */
		if (fieldType == IDB_TYPE_INT) {
			WORDSTR(tmp, idb_tabdata[i].iValue);
			if ((ret = data_write(fd, tmp, 2, &data_len)) < 0)
				return ret;
		}
		else
		if (fieldType == IDB_TYPE_LONG) {
			UINT32STR(tmp, idb_tabdata[i].lValue);
			if ((ret = data_write(fd, tmp, 4, &data_len)) < 0)
				return ret;

			//DPRINTF("\tLong value: %ld\n", idb_tabdata[i].lValue);
		}
		else
		if ((fieldType == IDB_TYPE_STR)
			|| (fieldType == IDB_TYPE_FILE)) {
			int bytes = 1;
			long len = (long)strlen( idb_tabdata[i].sValue );

			/* Basically if len > pow(2, bytes * 8) then bytes++ */
			if (len > 255)
				bytes = 2;
			if (len > 65535)
				bytes = 3;
			if (len > 16777215)
				bytes = 4;

			/* Save number of bytes used to store real size */
			tmp[0] = bytes;
			if ((ret = data_write(fd, tmp, 1, &data_len)) < 0)
				return ret;

			UINT32STR(tmp, len);
			DPRINTF("%s: Using %d bytes for saving size\n", __FUNCTION__, bytes);
			if ((ret = data_write(fd, tmp, bytes, &data_len)) < 0)
				return ret;

			if ((ret = data_write(fd, idb_tabdata[i].sValue, len, &data_len)) < 0)
				return ret;

			//DPRINTF("\tString or file value: %s\n", idb_tabdata[i].sValue);
		}
	}

	lseek(fd, orig_data_len, SEEK_SET);

	UINT32STR(tmp, (data_len - 3) );
	write(fd, tmp, 3);

	lseek(fd, orig_data_len + data_len, SEEK_SET);

	return data_len;
}

long _idb_read_header(int fd, char *filename)
{
	long i, j, idTab, idField;
	int inumf = 0, nf = 0;
	int ilen = 0, itype = 0, tabcnt = 0;
	long data_len = 0, size = 0;
	unsigned char *tmp = NULL;
	unsigned char *tmpc = NULL;
	unsigned char *tmpn = NULL;

	/* Get header size */
	tmp = data_fetch(fd, 3, &data_len, 1);
	if (tmp == NULL)
		return -EIO;
	size = (long)GETUINT32(tmp);
	DPRINTF("%s: Header size is %ld bytes\n", __FUNCTION__, size);
	free(tmp); tmp = NULL;

	/* Get number of tables */
	tmp = data_fetch(fd, 2, &data_len, 0);
	if (tmp == NULL)
		return -EIO;
	tabcnt = (int)GETWORD(tmp);
	DPRINTF("%s: Number of tables is %d\n", __FUNCTION__, tabcnt);
	free(tmp); tmp = NULL;

	//idb_init();

	idb_tables_num += tabcnt;
	idb_tables = realloc( idb_tables, idb_tables_num * sizeof(tTableDef) );

	for (i = 0; i < tabcnt; i++) {
		/* Get internal ID */
		tmp = data_fetch(fd, 4, &data_len, 0);
		if (tmp == NULL)
			return -EIO;
		idTab = (long)GETUINT32(tmp);
		DPRINTF("%s: Internal table ID is %ld\n", __FUNCTION__, idTab);
		free(tmp); tmp = NULL;

		/* Get name length */
		tmp = data_fetch(fd, 1, &data_len, 0);
		if (tmp == NULL)
			return -EIO;
		ilen = (int)GETBYTE(tmp);
		DPRINTF("%s: Name length is %d bytes\n", __FUNCTION__, ilen);
		free(tmp); tmp = NULL;

		/* Get table name */
		tmpn = data_fetch(fd, ilen, &data_len, 0);
		if (tmpn == NULL)
			return -EIO;
		tmpn[ilen] = 0;
		DPRINTF("%s: Name is '%s'\n", __FUNCTION__, tmpn);

		/* Get table comment length */
		tmp = data_fetch(fd, 1, &data_len, 0);
		if (tmp == NULL)
			return -EIO;
		ilen = (int)GETBYTE(tmp);
		DPRINTF("%s: Comment length is %d bytes\n", __FUNCTION__, ilen);

		/* Get table comment */
		if (ilen > 0) {
			tmpc = data_fetch(fd, ilen, &data_len, 0);
			if (tmpc == NULL)
				return -EIO;
			tmpc[ilen] = 0;
			DPRINTF("%s: Comment is '%s'\n", __FUNCTION__, tmpc);
		}

		idb_tables[i].id = idTab;
		idb_tables[i].name = strdup( (char *)tmpn);
		idb_tables[i].comment = ((tmpc != NULL) && (strlen((char *)tmpc) != 0)) ?
			strdup((char *)tmpc) : NULL;

		free(tmp); tmp = NULL;
		free(tmpc); tmpc = NULL;
		free(tmpn); tmpn = NULL;

		/* Number of fields */
		tmp = data_fetch(fd, 1, &data_len, 0);
		if (tmp == NULL)
			return -EIO;
		inumf = (int)GETBYTE(tmp);
		DPRINTF("%s: Num fields is %d\n", __FUNCTION__, inumf);
		free(tmp); tmp = NULL;

		nf = idb_fields_num + inumf;
		DPRINTF("%s: New field count is %d\n", __FUNCTION__, nf);
		idb_fields = realloc( idb_fields, (nf + 1) * sizeof(tTableFieldDef) );
		if (idb_fields == NULL) {
			DPRINTF("%s: Cannot reallocate idb_fields to %d\n", __FUNCTION__,
				idb_fields_num + inumf);
			return -ENOMEM;
		}

		/* Iterate all fields */
		for (j = 0; j < inumf; j++) {
			/* Get internal ID */
			tmp = data_fetch(fd, 4, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			idField = (long)GETUINT32(tmp);
			DPRINTF("%s: Internal field ID is %ld\n", __FUNCTION__, idField);
			free(tmp); tmp = NULL;

			idb_fields[idb_fields_num + j].id = idField;
			idb_fields[idb_fields_num + j].idTable = idTab;

			/* Get field name type */
			tmp = data_fetch(fd, 1, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			itype = (int)GETBYTE(tmp);
			DPRINTF("%s: Field type is %d (%s)\n", __FUNCTION__, itype,
				_idb_get_type(itype) );
			free(tmp); tmp = NULL;

			idb_fields[idb_fields_num + j].type = itype;

			/* Get field name length */
			tmp = data_fetch(fd, 1, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			ilen = (int)GETBYTE(tmp);
			DPRINTF("%s: Field name length is %d bytes\n", __FUNCTION__, ilen);
			free(tmp); tmp = NULL;

			/* Get field name */
			tmp = data_fetch(fd, ilen, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			tmp[ilen] = 0;
			DPRINTF("%s: Field name is '%s'\n", __FUNCTION__, tmp);
			idb_fields[idb_fields_num + j].name = strdup((char *)tmp);
			free(tmp); tmp = NULL;
		}

		idb_fields_num += inumf;
	}

	/* Check for size, 3 bytes allocated for header size so decrease by those 3 bytes for comparison */
	if (data_len - 3 != size)
		data_len = -EINVAL;

	return data_len;
}

long _idb_read_data(int fd)
{
	long i, fieldcnt, type, iValue;
	long data_len = 0, size = 0;
	long idField, idRow, idTab, lValue;
	unsigned char *tmp = NULL;

	/* Get header size */
	tmp = data_fetch(fd, 3, &data_len, 1);
	if (tmp == NULL)
		return -EIO;
	size = (long)GETUINT32(tmp);
	DPRINTF("%s: Data size is %ld bytes\n", __FUNCTION__, size);
	free(tmp); tmp = NULL;

	/* Get data field count */
	tmp = data_fetch(fd, 3, &data_len, 1);
	if (tmp == NULL)
		return -EIO;
	fieldcnt = (long)GETUINT32(tmp);
	DPRINTF("%s: Number of fields is %ld\n", __FUNCTION__, fieldcnt);
	free(tmp); tmp = NULL;

	if (idb_tabdata_num == 0) {
		idb_tabdata = NULL;

		idb_tabdata = malloc( fieldcnt * sizeof(tTableData) );
		memset( idb_tabdata, 0, fieldcnt * sizeof(tTableData) );
	}
	else {
		idb_tabdata = realloc( idb_tabdata, (idb_tabdata_num + fieldcnt) * sizeof(tTableData) );
		memset( idb_tabdata + (idb_tabdata_num * sizeof(tTableData)), 0, fieldcnt * sizeof(tTableData) );
	}

	for (i = 0; i < fieldcnt; i++) {
		/* Get the field type */
		tmp = data_fetch(fd, 1, &data_len, 0);
		if (tmp == NULL)
			return -EIO;
		type = (int)GETBYTE(tmp);
		DPRINTF("%s: Type is %ld (%s)\n", __FUNCTION__, type,
			_idb_get_type(type) );
		free(tmp); tmp = NULL;

		/* Get internal ID */
		tmp = data_fetch(fd, 4, &data_len, 0);
		if (tmp == NULL)
			return -EIO;
		idTab = (int)GETUINT32(tmp);
		idb_tabdata[idb_tabdata_num + i].id = (int)GETUINT32(tmp);
		DPRINTF("%s: Table ID is %ld\n", __FUNCTION__, idTab);
		free(tmp); tmp = NULL;

		/* Get the field ID */
		tmp = data_fetch(fd, 4, &data_len, 0);
		if (tmp == NULL)
			return -EIO;
		idField = (long)GETUINT32(tmp);
		idb_tabdata[idb_tabdata_num + i].idField = (int)GETUINT32(tmp);
		DPRINTF("%s: Field ID is %ld\n", __FUNCTION__, idField);
		free(tmp); tmp = NULL;

		/* Get the row ID */
		tmp = data_fetch(fd, 4, &data_len, 0);
		if (tmp == NULL)
			return -EIO;
		idRow = (long)GETUINT32(tmp);
		idb_tabdata[idb_tabdata_num + i].idRow = (int)GETUINT32(tmp);
		DPRINTF("%s: Row ID is %ld\n", __FUNCTION__, idRow);
		free(tmp); tmp = NULL;

		/* Get the data themselves */
		if (type == IDB_TYPE_INT) {
			tmp = data_fetch(fd, 2, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			iValue = (int)GETWORD(tmp);
			idb_tabdata[idb_tabdata_num + i].iValue = iValue;
			DPRINTF("%s: Got iValue of %ld\n", __FUNCTION__, iValue);
			free(tmp); tmp = NULL;
		}
		else
		if (type == IDB_TYPE_LONG) {
			tmp = data_fetch(fd, 4, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			lValue = (long)GETUINT32(tmp);
			idb_tabdata[idb_tabdata_num + i].lValue = lValue;
			DPRINTF("%s: Got lValue of %ld\n", __FUNCTION__, lValue);
			free(tmp); tmp = NULL;
		}
		else
		if ((type == IDB_TYPE_STR)
			|| (type == IDB_TYPE_FILE)) {
			int bytes;

			tmp = data_fetch(fd, 1, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			bytes = (int)GETWORD(tmp);
			free(tmp); tmp = NULL;

			tmp = data_fetch(fd, bytes, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			size = (long)GETUINT32(tmp);
			DPRINTF("%s: Got size of %ld\n", __FUNCTION__, size);
			free(tmp); tmp = NULL;

			tmp = data_fetch(fd, size, &data_len, 0);
			if (tmp == NULL)
				return -EIO;
			DPRINTF("%s: Got string data of '%s'\n", __FUNCTION__, tmp);
			idb_tabdata[idb_tabdata_num + i].sValue = strdup((char *)tmp);
			free(tmp); tmp = NULL;
		}
	}
	idb_tabdata_num += fieldcnt;

	return data_len;
}

int idb_load(char *filename)
{
	long len;
	int fd, ret = -EINVAL;
	long data_len = 0;
	unsigned char *tmp = NULL;

	if (filename == NULL)
		return ret;

	_idb_filename = strdup(filename);

	DPRINTF("%s: Loading file '%s'\n", __FUNCTION__, filename);

	_idb_session_start = idb_get_time( TIME_CURRENT );

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return -errno;

	/* Read the CDVDB header */
	tmp = data_fetch(fd, 5, &data_len, 0);
	if (tmp == NULL)
		goto cleanup;
	if (strcmp((const char *)tmp, "CDVDB") != 0)
		goto cleanup;

	if ((len = _idb_read_header(fd, filename)) < 0)
		goto cleanup;
	data_len += len;

	if ((len = _idb_read_data(fd)) < 0)
		goto cleanup;
	data_len += len;

	ret = (get_file_size(filename) != data_len) ? -EIO : 0;

	if (ret == 0)
		DPRINTF("%s: File '%s' loaded, size is %ld bytes\n", __FUNCTION__,
			filename, data_len);
	else
		DPRINTF("%s: Error on loading file '%s'\n", __FUNCTION__,
			filename);
cleanup:
	close(fd);

	return ret;
}

int idb_save(char *filename)
{
	int fd;
	long ret = 0;
	long data_len = 0;

	if (filename == NULL)
		filename = _idb_filename;
	if (filename == NULL)
		return -EINVAL;

	DPRINTF("%s: Saving file '%s'\n", __FUNCTION__, filename);

	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return -EPERM;

	if ((ret = _idb_save_header(fd)) < 0)
		return -ret;
	data_len += ret;
	if ((ret = _idb_save_data(fd, data_len)) < 0)
		return -ret;
	data_len += ret;

	close(fd);

	DPRINTF("%s: File '%s' saved, size is %ld bytes\n", __FUNCTION__,
		filename, data_len);

	return 0;
}

#else
int idb_init(void) {};
void idb_free(void) {};
long idb_table_id(char *name) {};
int idb_table_create(char *name, int num_fields, void *fields, char *comment) {};
int idb_table_insert(char *name, int num_data, tTableDataInput *td) {};
int idb_table_update_row(char *table_name, long idRow, int num_data, tTableDataInput *td) {};
int idb_table_update(char *table_name, int num_data, tTableDataInput *td, int num_where_fields, tTableDataInput *where_fields);
int idb_table_delete_row(char *table_name, long idRow) {};
int idb_table_delete(char *table_name, int num_where_fields, tTableDataInput *where_fields) {};
int idb_table_drop(char *table_name) {};
void idb_results_dump(tTableDataSelect tds) {};
void idb_results_free(tTableDataSelect tds) {};
void idb_dump(void) {};
int idb_type_id(char *type) {};
int idb_load(char *filename) {};
int idb_save(char *filename) {};
char *idb_get_filename(void) {};
int idb_query(char *query) {};
tTableDataSelect idb_get_last_select_data(void) {};
void idb_free_last_select_data(void) {};
#endif

