
uses "Node"

Hierarchy "default" {
    Resource{ "cluster", name = "hype",
    children = { ListOf{ Node,
                  ids = "201-354",
                  args = { basename = "hype",
                           sockets = {"0-7", "8-15"},
                           memory_per_socket = 15000 }
                 },
               }
    }
}

Hierarchy "power" {
    Resource{ "powerpanel", name="ppnl", size = 1000, children = {
      Resource{ "pdu", name = "pdu1", id = "1", size = 500,
        children = { ListOf{ Node,
                       ids = "201-299",
                       args = { basename = "hype",
                                sockets = { "0" },
                                size = 10 }
                     }
                   }
        },
      Resource{ "pdu", name = "pdu2", id = "2", size = 500,
        children = { ListOf{ Node,
                      ids = "300-354",
                      args = { basename = "hype",
                            sockets = { "0" },
                            size = 20  }
                     }
                   }
        }
      }}
}

Hierarchy "bandwidth" {
    Resource{ "filesystem", name="pfs", size = 90000, children = {
      Resource{ "gateway", name = "gateway_node_pool", size = 90000, children = {
        Resource{ "core_switch", name = "core_switch_pool", size = 144000, children = {
          Resource{ "switch", name = "edge_switch1",  id = "1",size= 72000,
            children = { ListOf{ Node,
                         ids = "201-299",
                         args = { basename = "hype",
                                  sockets = { "0" },
                                  size = 4000 }
                           }
                       }
            },
          Resource{ "switch", name = "edge_switch2",  id = "2", size= 72000,
            children = { ListOf{ Node,
                         ids = "300-354",
                         args = { basename = "hype",
                                  sockets = { "0" },
                                  size = 4000  }
                           }
                       }
            }
        }}
      }}
    }}
}
