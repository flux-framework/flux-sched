
uses "Node"

Hierarchy "default" {
    Resource{ "cluster", name = "cab",
    children = { ListOf{ Node,
                  ids = "1235-1238",
                  args = { name = "cab", sockets = {"0-7"} }
                 },
               }
    }
}
