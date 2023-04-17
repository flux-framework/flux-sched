##############################################################
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
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
        properties,
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
        properties -- Comma-separated list of property strings
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
                "properties": properties,
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
        self._uniqId = 0
        self._rv1NoSched = rv1
        self._encode()

    def _tick_uniq_id(self):
        self._uniqId += 1

    def _add_and_tick_uniq_id(self, vtx, edg=None):
        self.add_node(vtx)
        if edg:
            self.add_edge(edg)
        self._tick_uniq_id()

    def _extract_id_from_hn(self, hostName):
        postfix = re.findall(r"(\d+$)", hostName)
        rc = self._uniqId
        if len(postfix) == 1:
            rc = int(postfix[0])
        return rc

    def _add_to_rankdict(self, key, gLabel, rankdict):
        if key in rankdict:
            rankdict[key].append(gLabel)
        else:
            rankdict[key] = [gLabel]

    def _contains_any(self, prop_str, charset):
        for c in charset:
            if c in prop_str:
                return True
        return False

    def _per_rank_property_dict(self, properties, rankdict):
        for p in properties:
            # This can be extended later to support scheduler specific
            # string (@suffix)
            if self._contains_any(p, "!&'\"^`|()"):
                raise ValueError(f"invalid character used in property={p}")
            self._add_to_rankdict("node", p, rankdict)

    def _encode_child(self, ppid, hPath, rank, resType, i, rpd):
        path = f"{hPath}/{resType}{i}"
        properties = []
        # This can be extended later to support fine grained property
        # attachment using rpd
        vtx = FluxionResourcePoolV1(
            self._uniqId,
            resType,
            resType,
            resType + str(i),
            i,
            self._uniqId,
            rank,
            True,
            "",
            1,
            properties,
            path,
        )
        edg = FluxionResourceRelationshipV1(ppid, vtx.get_id())
        self._add_and_tick_uniq_id(vtx, edg)

    def _encode_rank(self, ppid, rank, children, hList, rdict, rpd):
        if rdict[rank] >= len(hList):
            raise ValueError(f"nodelist doesn't include node for rank={rank}")
        hPath = f"/cluster0/{hList[rdict[rank]]}"
        iden = self._extract_id_from_hn(hList[rdict[rank]])
        properties = []
        if "node" in rpd:
            properties = rpd["node"]
        vtx = FluxionResourcePoolV1(
            self._uniqId,
            "node",
            "node",
            hList[rdict[rank]],
            iden,
            self._uniqId,
            rank,
            True,
            "",
            1,
            properties,
            hPath,
        )
        edg = FluxionResourceRelationshipV1(ppid, vtx.get_id())
        self._add_and_tick_uniq_id(vtx, edg)
        for key, val in children.items():
            for i in IDset(val):
                self._encode_child(vtx.get_id(), hPath, rank, str(key), i, rpd)

    def _encode_rlite(self, ppid, entry, hList, rdict, pdict):
        for rank in list(IDset(entry["rank"])):
            rpd = {}
            if rank in pdict:
                self._per_rank_property_dict(pdict[rank], rpd)
            self._encode_rank(ppid, rank, entry["children"], hList, rdict, rpd)

    def _encode(self):
        hList = Hostlist(self._rv1NoSched["execution"]["nodelist"])
        vtx = FluxionResourcePoolV1(
            self._uniqId,
            "cluster",
            "cluster",
            "cluster0",
            0,
            self._uniqId,
            -1,
            True,
            "",
            1,
            [],
            "/cluster0",
        )
        self._add_and_tick_uniq_id(vtx, None)
        i = 0
        rdict = {}
        for entry in self._rv1NoSched["execution"]["R_lite"]:
            for rank in list(IDset(entry["rank"])):
                if rank in rdict:
                    raise ValueError(f"R_lite: rank={rank} found again!")
                rdict[rank] = i
                i += 1
        props = []
        if "properties" in self._rv1NoSched["execution"]:
            props = self._rv1NoSched["execution"]["properties"]
        pdict = {}
        for p in props:
            for rank in list(IDset(props[p])):
                if rank in pdict:
                    pdict[rank].append(p)
                else:
                    pdict[rank] = [p]

        for entry in self._rv1NoSched["execution"]["R_lite"]:
            self._encode_rlite(vtx.get_id(), entry, hList, rdict, pdict)
