uses "Node"
Hierarchy "default"
{
   Resource{ "filesystem", name="pfs", tags = { max_bw = 48000 }, children = {
      Resource {"gateway", name = "gateway_node_pool", tags = { max_bw = 48000 }, children = {
         Resource {"core_switch", name = "core_switch_pool", tags = { max_bw = 64000 }, children = {
            Resource {"switch", name = "edge_switch1", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1-18", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch2", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "19-36", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch3", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "37-54", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch4", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "55-72", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch5", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "73-90", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch6", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "91-108", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch7", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "109-126", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch8", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "127-144", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch9", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "145-160", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch10", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "163-180", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch11", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "181-198", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch12", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "199-216", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch13", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "217-234", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch14", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "235-252", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch15", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "253-270", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch16", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "271-288", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch17", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "289-306", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch18", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "307-322", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch19", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "325-342", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch20", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "343-360", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch21", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "361-378", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch22", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "379-396", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch23", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "397-414", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch24", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "415-432", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch25", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "433-450", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch26", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "451-468", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch27", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "469-484", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch28", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "487-504", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch29", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "505-522", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch30", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "523-540", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch31", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "541-558", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch32", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "559-576", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch33", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "577-594", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch34", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "595-612", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch35", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "613-630", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch36", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "631-646", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch37", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "649-666", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch38", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "667-684", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch39", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "685-702", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch40", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "703-720", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch41", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "721-738", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch42", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "739-756", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch43", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "757-774", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch44", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "775-792", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch45", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "793-808", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch46", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "811-828", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch47", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "829-846", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch48", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "847-864", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch49", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "865-882", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch50", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "883-900", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch51", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "901-918", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch52", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "919-936", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch53", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "937-954", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch54", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "955-970", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch55", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "973-990", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch56", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "991-1008", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch57", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1009-1026", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch58", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1027-1044", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch59", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1045-1062", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch60", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1063-1080", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch61", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1081-1098", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch62", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1099-1116", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch63", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1117-1132", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch64", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1135-1152", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch65", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1153-1170", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch66", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1171-1188", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch67", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1189-1206", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch68", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1207-1224", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch69", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1225-1242", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch70", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1243-1260", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch71", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1261-1278", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch72", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1279-1294", args = { name = "cab", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }}
         }}
      }}
   }}
}
