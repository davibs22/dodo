#ifndef DODO_CORE_REPOSITORY_NETWORK_REPO_H
#define DODO_CORE_REPOSITORY_NETWORK_REPO_H

#include "../types/network.h"

#ifdef __cplusplus
extern "C" {
#endif

DodoNetworkList* dodo_network_list_fetch(void);

#ifdef __cplusplus
}
#endif

#endif
