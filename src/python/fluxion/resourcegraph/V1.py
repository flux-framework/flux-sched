##############################################################
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, COPYING)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

import re

from flux.idset import IDset
from flux.hostlist import Hostlist
from fluxion.jsongraph.objects.graph import Graph, Node, Edge

class FluxionResourcePoolV1(Node):

    """
    Fluxion Resource Pool Vertex Class: extend jsongraph's Node class
    """

    def __init__(
        self,
        vtxId,
        resType,
        basename,
        name,
        iden,
        uniqId,
        rank,
        exclusive,
        unit,
        size,
        path,
    ):
        """Constructor

        vtxId -- Vertex Id
        resType -- Resource Pool type (e.g., core or gpu): because V1 only
                support singleton resource, the following omits the term pool.
        basename -- Resource basename (e.g., core or gpu)
        name -- Resource name (e.g., core2 if basename is core and Id is 2)
        iden -- Resource Id (e.g., 2 if name is core2)
        uniqId -- Unique Id of this resource vertex
        rank -- Flux broker rank to which this vertex belongs
        exclusive -- Exclusivity
        unit -- Unit of this resource
        size -- Amount of individual resources in this resource pool in unit
        paths -- Fully qualified paths dictionary
        """
        if not self.constraints(resType):
            raise ValueError(f"resource type={resType} unsupported by RV1")

        super(FluxionResourcePoolV1, self).__init__(
            vtxId,
            metadata={
                "type": resType,
                "basename": basename,
                "name": name,
                "id": iden,
                "uniq_id": uniqId,
                "rank": rank,
                "exclusive": exclusive,
                "unit": unit,
                "size": size,
                "paths": {"containment": path},
            },
        )

    @staticmethod
    def constraints(resType):
        return resType in ["cluster", "node", "core", "gpu"]


class FluxionResourceRelationshipV1(Edge):
    """
    Fluxion Resource Relationship V1 Class: extend jsongraph's Edge class
    """

    def __init__(self, parentId, vtxId):
        """Constructor
        parentId -- Parent vertex Id
        vtxId -- Child vertex Id
        """
        super(FluxionResourceRelationshipV1, self).__init__(
            parentId,
            vtxId,
            directed=True,
            metadata={"name": {"containment": "contains"}},
        )


class FluxionResourceGraphV1(Graph):
    """
    Fluxion Resource Graph Version 1:  extend jsongraph's Graph class
    """

    def __init__(self, rv1):
        """Constructor
        rv1 -- RV1 Dictorary that conforms to Flux RFC 20:
                   Resource Set Specification Version 1
        """
        super(FluxionResourceGraphV1, self).__init__()
        self.__uniqId = 0
        self.__rv1NoSched = rv1
        self.__encode()

    def __tick_uniq_id(self):
        self.__uniqId += 1

    def __add_and_tick_uniq_id(self, vtx, edg):
        self.add_node(vtx)
        if edg:
            self.add_edge(edg)
        self.__tick_uniq_id()

    def __extract_id_from_hn(self, hostName):
        postfix = re.findall(r"(\d+$)", hostName)
        rc = self.__uniqId
        if len(postfix) == 1:
            rc = int(postfix[0])
        return rc

    def __encode_child(self, ppid, hPath, rank, resType, i):
        path = f"{hPath}/{resType}{i}"
        vtx = FluxionResourcePoolV1(
            self.__uniqId,
            resType,
            resType,
            resType + str(i),
            i,
            self.__uniqId,
            rank,
            True,
            "",
            1,
            path,
        )
        edg = FluxionResourceRelationshipV1(ppid, vtx.get_id())
        self.__add_and_tick_uniq_id(vtx, edg)

    def __encode_rank(self, ppid, rank, children, hList, hIndex):
        hPath = f"/cluster0/{hList[hIndex]}"
        iden = self.__extract_id_from_hn(hList[hIndex])
        vtx = FluxionResourcePoolV1(
            self.__uniqId,
            "node",
            "node",
            hList[hIndex],
            iden,
            self.__uniqId,
            rank,
            True,
            "",
            1,
            hPath,
        )
        edg = FluxionResourceRelationshipV1(ppid, vtx.get_id())
        self.__add_and_tick_uniq_id(vtx, edg)
        for key, val in children.items():
            for i in IDset(val):
                self.__encode_child(vtx.get_id(), hPath, rank, str(key), i)

    def __encode_rlite(self, ppid, entry, hList, hIndex):
        for rank in list(IDset(entry["rank"])):
            hIndex += 1
            self.__encode_rank(ppid, rank, entry["children"], hList, hIndex)
        return hIndex

    def __encode(self):
        hIndex = -1
        hList = Hostlist(self.__rv1NoSched["execution"]["nodelist"])
        vtx = FluxionResourcePoolV1(
            self.__uniqId,
            "cluster",
            "cluster",
            "cluster0",
            0,
            self.__uniqId,
            -1,
            True,
            "",
            1,
            "/cluster0",
        )
        self.__add_and_tick_uniq_id(vtx, None)
        for entry in self.__rv1NoSched["execution"]["R_lite"]:
            hIndex = self.__encode_rlite(vtx.get_id(), entry, hList, hIndex)
