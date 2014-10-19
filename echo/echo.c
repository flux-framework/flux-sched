#include <stdio.h>
#include <json.h>
#include <flux/core.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/jsonutil.h"
#include "src/common/libutil/monotime.h"

int main (int ac, char **av)
{
	int i, id, ncopies = 1;
	char *tag;
	const char *s;
	struct timespec ts0;
	flux_t h = flux_api_open ();

	if (!h) {
		fprintf (stderr, "Failed to open connection to cmb!\n");
		exit (1);
	}
	json_object *o = json_object_new_object ();

	if (ac < 2) {
		fprintf (stderr, "Usage: echo string [ncopies]\n");
		exit (1);
	}
	if (ac >= 3)
		ncopies = atoi(av[2]);

	util_json_object_add_int (o, "repeat", ncopies);
	util_json_object_add_string (o, "string", av[1]);

	monotime (&ts0);
	if (flux_request_send (h, o, "echo") < 0) {
		fprintf (stderr, "flux_request_send failed!\n");
		exit (1);
	}


	for (i = 0; i < ncopies; i++) {
		zmsg_t *zmsg;
		double ms;

		if (!(zmsg = flux_response_recvmsg (h, false))) {
			fprintf (stderr, "Failed to recv zmsg!\n");
			exit (1);
		}
		ms = monotime_since (ts0);
		//zmsg_dump_compact (zmsg);

		if (flux_msg_decode (zmsg, &tag, &o) < 0) {
			fprintf (stderr, "flux_msg_decode failed!\n");
			exit (1);
		}


		if (util_json_object_get_string (o, "string", &s) < 0) {
			fprintf (stderr, "get string failed!\n");
			fprintf (stderr, "Got:\n%s\n",
					json_object_to_json_string (o));
			exit (1);
		}

		util_json_object_get_int (o, "id", &id);
		fprintf (stderr, "%0.3fms: got reply %d from %d: %s\n",
			ms, i+1, id, s);
		zmsg_destroy (&zmsg);
	}

	exit(0);
}

