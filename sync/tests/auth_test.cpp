// Exercises the loopback redirect listener with real sockets.
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "../GoogleAuth.h"
int main(int argc, char** argv) {
	GoogleAuth::LoopbackFlow flow; std::string err, code;
	if (!flow.Start(err)) { printf("start failed: %s\n", err.c_str()); return 1; }
	std::string url = flow.AuthorizationUrl("CLIENT.apps.googleusercontent.com",
		GoogleAuth::kScopes);
	std::string q = url.substr(url.find('?') + 1);
	FILE* f = fopen(argv[1], "w");
	fprintf(f, "%s %s\n", flow.RedirectUri().c_str(),
		GoogleAuth::QueryParam(q, "state").c_str()); fclose(f);
	printf("auth url: %.160s...\n", url.c_str());
	bool ok = flow.WaitForCode(15, NULL, code, err);
	printf("result: ok=%d code='%s' err='%s'\n", ok, code.c_str(), err.c_str());
	return 0;
}
