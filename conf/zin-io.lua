uses "Node"
Hierarchy "default"
{
   Resource{ "filesystem", name="pfs", tags = { max_bw = 90000 }, children = {
      Resource {"gateway", name = "gateway_node_pool", tags = { max_bw = 90000 }, children = {
         Resource {"core_switch", name = "core_switch_pool", tags = { max_bw = 144000 }, children = {
            Resource {"switch", name = "edge_switch1", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1-18", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch2", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "19-36", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch3", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "37-54", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch4", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "55-72", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch5", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "73-90", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch6", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "91-108", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch7", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "109-126", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch8", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "127-144", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch9", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "145-160", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch10", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "163-180", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch11", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "181-198", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch12", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "199-216", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch13", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "217-234", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch14", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "235-252", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch15", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "253-270", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch16", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "271-288", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch17", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "289-306", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch18", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "307-322", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch19", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "325-342", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch20", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "343-360", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch21", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "361-378", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch22", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "379-396", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch23", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "397-414", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch24", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "415-432", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch25", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "433-450", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch26", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "451-468", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch27", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "469-484", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch28", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "487-504", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch29", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "505-522", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch30", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "523-540", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch31", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "541-558", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch32", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "559-576", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch33", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "577-594", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch34", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "595-612", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch35", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "613-630", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch36", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "631-646", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch37", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "649-666", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch38", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "667-684", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch39", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "685-702", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch40", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "703-720", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch41", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "721-738", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch42", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "739-756", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch43", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "757-774", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch44", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "775-792", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch45", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "793-808", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch46", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "811-828", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch47", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "829-846", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch48", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "847-864", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch49", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "865-882", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch50", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "883-900", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch51", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "901-918", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch52", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "919-936", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch53", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "937-954", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch54", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "955-970", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch55", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "973-990", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch56", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "991-1008", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch57", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1009-1026", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch58", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1027-1044", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch59", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1045-1062", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch60", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1063-1080", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch61", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1081-1098", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch62", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1099-1116", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch63", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1117-1132", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch64", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1135-1152", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch65", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1153-1170", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch66", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1171-1188", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch67", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1189-1206", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch68", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1207-1224", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch69", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1225-1242", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch70", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1243-1260", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch71", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1261-1278", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch72", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1279-1294", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch73", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1297-1314", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch74", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1315-1332", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch75", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1333-1350", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch76", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1351-1368", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch77", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1369-1386", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch78", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1387-1404", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch79", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1405-1422", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch80", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1423-1440", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch81", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1441-1456", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch82", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1459-1476", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch83", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1477-1494", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch84", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1495-1512", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch85", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1513-1530", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch86", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1531-1548", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch87", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1549-1566", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch88", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1567-1584", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch89", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1585-1602", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch90", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1603-1618", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch91", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1621-1638", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch92", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1639-1656", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch93", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1657-1674", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch94", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1675-1692", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch95", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1693-1710", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch96", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1711-1728", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch97", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1729-1746", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch98", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1747-1764", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch99", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1765-1780", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch100", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1783-1800", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch101", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1801-1818", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch102", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1819-1836", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch103", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1837-1854", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch104", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1855-1872", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch105", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1873-1890", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch106", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1891-1908", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch107", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1909-1926", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch108", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1927-1942", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch109", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1945-1962", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch110", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1963-1980", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch111", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1981-1998", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch112", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "1999-2016", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch113", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2017-2034", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch114", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2035-2052", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch115", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2053-2070", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch116", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2071-2088", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch117", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2089-2104", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch118", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2107-2124", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch119", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2125-2142", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch120", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2143-2160", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch121", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2161-2178", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch122", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2179-2196", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch123", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2197-2214", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch124", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2215-2232", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch125", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2233-2250", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch126", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2251-2266", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch127", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2269-2286", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch128", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2287-2304", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch129", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2305-2322", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch130", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2323-2340", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch131", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2341-2358", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch132", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2359-2376", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch133", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2377-2394", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch134", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2395-2412", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch135", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2413-2428", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch136", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2431-2448", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch137", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2449-2466", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch138", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2467-2484", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch139", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2485-2502", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch140", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2503-2520", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch141", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2521-2538", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch142", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2539-2556", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch143", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2557-2574", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch144", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2575-2590", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch145", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2593-2610", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch146", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2611-2628", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch147", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2629-2646", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch148", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2647-2664", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch149", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2665-2682", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch150", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2683-2700", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch151", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2701-2718", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch152", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2719-2736", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch153", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2737-2752", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch154", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2755-2772", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch155", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2773-2790", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch156", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2791-2808", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch157", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2809-2826", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch158", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2827-2844", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch159", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2845-2862", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch160", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2863-2880", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch161", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2881-2898", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }},
            Resource {"switch", name = "edge_switch162", tags = { max_bw = 72000 }, children = {
               ListOf{ Node, ids = "2899-2914", args = { name = "zin", sockets = {"0-7", "8-15"}, tags = { max_bw = 4000 }}}
            }}    
         }}
      }}
   }}
}
