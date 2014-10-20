
uses "Node"

Hierarchy "default" {
    Resource{ "cluster", name = "cab",
    children = { ListOf{ Node,
                  ids = "1-1232",
                  args = { name = "cab", sockets = {"0-7", "8-15"} }
                 },
               }
    }
}
