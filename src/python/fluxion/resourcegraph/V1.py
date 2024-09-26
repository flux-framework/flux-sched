##############################################################
# Copyright 2021 Lawrence Livermore National Security, LLC
# (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
#
# This file is part of the Flux resource manager framework.
# For details, see https://github.com/flux-framework.
#
# SPDX-License-Identifier: LGPL-3.0
##############################################################

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
        basename=None,
        name=None,
        iden=None,
        uniqId=None,
        rank=None,
        exclusive=None,
        unit=None,
        size=None,
        properties=None,
        path=None,
        status=0,
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
        properties -- mapping from property name to value
        paths -- Fully qualified paths dictionary
        status -- Resource status (0 for 'up', 1 for 'down'), defaults to 0
        """
        if not self.constraints(resType):
            raise ValueError(f"resource type={resType} unsupported by RV1")
        metadata = {
            "type": resType,
        }
        if basename is not None:
            metadata["basename"] = basename
        if name is not None:
            metadata["name"] = name
        if iden is not None:
            metadata["id"] = iden
        if uniqId is not None:
            metadata["uniq_id"] = uniqId
        if rank is not None:
            metadata["rank"] = rank
        if exclusive is not None:
            metadata["exclusive"] = exclusive
        if unit:
            metadata["unit"] = unit
        if size is not None:
            metadata["size"] = size
        if properties:
            metadata["properties"] = properties
        if path is not None:
            metadata["paths"] = {"containment": path}
        else:
            raise ValueError("path must not be None")
        if status != 0:
            metadata["status"] = status
        super().__init__(vtxId, metadata=metadata)

    @staticmethod
    def constraints(resType):
        return resType in ["cluster", "node", "core", "gpu"]


class FluxionResourceRelationshipV1(Edge):
    """
    Fluxion Resource Relationship V1 Class: extend jsongraph's Edge class
    """


class FluxionResourceGraphV1(Graph):
    """
    Fluxion Resource Graph Version 1:  extend jsongraph's Graph class
    """

    def __init__(self, rv1):
        """Constructor
        rv1 -- RV1 Dictorary that conforms to Flux RFC 20:
                   Resource Set Specification Version 1
        """
        super().__init__(directed=False)
        # graph *is* directed, however, suppress unnecessary `"directed": true` fields
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

    def _contains_any(self, prop_str, charset):
        for c in charset:
            if c in prop_str:
                return True
        return False

    def _encode_child(self, ppid, path, rank, resType, i):
        vtx = FluxionResourcePoolV1(
            self._uniqId, resType, iden=i, rank=rank, path=f"{path}/{resType}{i}"
        )
        edg = FluxionResourceRelationshipV1(ppid, vtx.get_id())
        self._add_and_tick_uniq_id(vtx, edg)

    def _encode_rank(self, ppid, path, rank, children, hostname, properties):
        path = f"{path}/{hostname}"
        vtx = FluxionResourcePoolV1(
            self._uniqId,
            "node",
            name=hostname,
            rank=rank,
            properties=properties,
            path=path,
        )
        edg = FluxionResourceRelationshipV1(ppid, vtx.get_id())
        self._add_and_tick_uniq_id(vtx, edg)
        for key, val in children.items():
            for i in IDset(val):
                self._encode_child(vtx.get_id(), path, rank, str(key), i)

    def _encode_rlite(self, ppid, path, entry, hList, rdict, pdict):
        for rank in list(IDset(entry["rank"])):
            if rdict[rank] >= len(hList):
                raise ValueError(f"nodelist doesn't include node for rank={rank}")
            hostname = hList[rdict[rank]]
            self._encode_rank(
                ppid, path, rank, entry["children"], hostname, pdict.get(rank)
            )

    def _encode(self):
        hList = Hostlist(self._rv1NoSched["execution"]["nodelist"])
        path = "/cluster0"
        vtx = FluxionResourcePoolV1(
            self._uniqId,
            "cluster",
            path=path,
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
            if self._contains_any(p, "!&'\"^`|()"):
                raise ValueError(f"invalid character used in property={p}")
            for rank in list(IDset(props[p])):
                if rank in pdict:
                    pdict[rank][p] = ""
                else:
                    pdict[rank] = {p: ""}
        for entry in self._rv1NoSched["execution"]["R_lite"]:
            self._encode_rlite(vtx.get_id(), path, entry, hList, rdict, pdict)
