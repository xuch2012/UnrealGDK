
#pragma once

#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("Spatial Networking"), STATGROUP_SpatialNet, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net Actors Updated"),STAT_SpatialNetActorsUpdated,STATGROUP_SpatialNet );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net New Actors Sent"),STAT_SpatialNetNewActorsReplicated,STATGROUP_SpatialNet );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net Spatial Ops Received"),STAT_SpatialNetOpsReceived,STATGROUP_SpatialNet );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net Rpcs Sent"),STAT_SpatialNetRpcsSent,STATGROUP_SpatialNet );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net Rpcs Received"),STAT_SpatialNetRpcsReceived,STATGROUP_SpatialNet );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net Multicast Rpcs Sent"),STAT_SpatialNetMulticastRpcsSent,STATGROUP_SpatialNet );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net Multicast Rpcs Received"),STAT_SpatialNetMulticastRpcsReceived,STATGROUP_SpatialNet );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net Component Updates Sent"),STAT_SpatialNetComponentUpdatesSent,STATGROUP_SpatialNet );
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Net Component Updates Received"),STAT_SpatialNetComponentUpdatesReceived,STATGROUP_SpatialNet );
