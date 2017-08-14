-- Cluster with 3 pods, 4 switches per pod, 8 nodes per switch, 96 cores total
-- Generated via the following R commands
-- x <- data.frame(paste0("hype",seq(1,96)),
--                 rep(0,96), rep(0:2,each=32), rep(0:11,each=8))
-- colnames(x) <- c("3", "", "", "")
-- write.table(x, row.names=FALSE, sep=" ", file="topo/hype.topo", quote=FALSE)
-- commandArgs <- function(unused) "hype"
-- source("gen_conf.R") # gen_conf.R can be found at
-- # https://lc.llnl.gov/bitbucket/projects/STATE/repos/schedu-tron/browse

uses "Node"
Hierarchy "default" {
Resource { "cluster", name = "hype",
children = {
	Resource { "pod", name = "pod0", children = {
		Resource { "switch", name = "l2_switch0", children = {
			ListOf { Node,
			ids = "1,2,3,4,5,6,7,8",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch1", children = {
			ListOf { Node,
			ids = "9,10,11,12,13,14,15,16",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch2", children = {
			ListOf { Node,
			ids = "17,18,19,20,21,22,23,24",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch3", children = {
			ListOf { Node,
			ids = "25,26,27,28,29,30,31,32",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
	}}, -- pod
	Resource { "pod", name = "pod1", children = {
		Resource { "switch", name = "l2_switch4", children = {
			ListOf { Node,
			ids = "33,34,35,36,37,38,39,40",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch5", children = {
			ListOf { Node,
			ids = "41,42,43,44,45,46,47,48",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch6", children = {
			ListOf { Node,
			ids = "49,50,51,52,53,54,55,56",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch7", children = {
			ListOf { Node,
			ids = "57,58,59,60,61,62,63,64",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
	}}, -- pod
	Resource { "pod", name = "pod2", children = {
		Resource { "switch", name = "l2_switch8", children = {
			ListOf { Node,
			ids = "65,66,67,68,69,70,71,72",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch9", children = {
			ListOf { Node,
			ids = "73,74,75,76,77,78,79,80",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch10", children = {
			ListOf { Node,
			ids = "81,82,83,84,85,86,87,88",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
		Resource { "switch", name = "l2_switch11", children = {
			ListOf { Node,
			ids = "89,90,91,92,93,94,95,96",
			args = {
				basename = "hype",
				sockets = { "0-7,8-15" },
				memory_per_socket = 16000
			}} -- Node
		}}, -- l2_switch
	}}, -- pod

} -- children
} -- Resource
} -- Hierarchy
