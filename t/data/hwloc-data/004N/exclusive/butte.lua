uses "Node"
GPUNode = Node:subclass ("GPUNode")
function GPUNode:initialize (arg)
    assert (arg.gpus, 'Required GPUNode arg gpus missing')
    Node.initialize (self, arg)
    for i = 1, arg.gpus do
        self:add_child (Resource{ "gpu", id = i })
    end
end

Hierarchy "default" {
   Resource{ "cluster", name = "butte",
     children = { ListOf{ GPUNode,
         ids = "0-3",
         args = {
           basename = "butte",
           sockets = {"0-15", "16-31"},
           gpus = 4,
         },
       }
     }
   }
}
