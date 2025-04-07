#!/bin/sh

test_description='Test flux ion-R Utility'

. $(dirname $0)/sharness.sh

print_schema (){
    jq -r '.graph.nodes[].metadata | "\(.paths.containment) \(.rank)"' $1 > $2
}

print_schema2 (){
    jq -r '.graph.nodes[].metadata | "\(.paths.containment) \(.rank) \(.properties)"' $1 > $2
}

test_expect_success 'fluxion-R: encoding nodelists on heterogeneity works' '
    cat <<-EOF >expected1 &&
	/cluster0 null
	/cluster0/foo2 0
	/cluster0/foo2/core0 0
	/cluster0/foo2/core1 0
	/cluster0/foo3 2
	/cluster0/foo3/core0 2
	/cluster0/foo3/core1 2
	/cluster0/foo1 3
	/cluster0/foo1/core0 3
	/cluster0/foo1/core1 3
	/cluster0/foo4 1
	/cluster0/foo4/core0 1
	EOF
    flux R encode -r 0 -c 0-1 -H foo2 > out1 &&
    flux R encode -r 1 -c 0 -H foo3 >> out1 &&
    flux R encode -r 2-3 -c 0-1 -H foo[1,4] >> out1 &&
    cat out1 | flux R append > combined1.json &&
    cat combined1.json | flux ion-R encode > augmented1.json &&
    jq .scheduling augmented1.json > jgf1.json &&
    print_schema jgf1.json paths1 &&
    test_cmp expected1 paths1
'

test_expect_success 'fluxion-R: encoding nodelists with high ranks' '
    cat <<-EOF >expected2 &&
	/cluster0 null
	/cluster0/fluke82 79
	/cluster0/fluke82/core0 79
	/cluster0/fluke82/core1 79
	/cluster0/fluke82/core2 79
	/cluster0/fluke82/core3 79
	/cluster0/fluke83 80
	/cluster0/fluke83/core0 80
	/cluster0/fluke83/core1 80
	/cluster0/fluke83/core2 80
	/cluster0/fluke83/core3 80
	/cluster0/fluke84 81
	/cluster0/fluke84/core0 81
	/cluster0/fluke84/core1 81
	/cluster0/fluke84/core2 81
	/cluster0/fluke84/core3 81
	/cluster0/fluke85 82
	/cluster0/fluke85/core0 82
	/cluster0/fluke85/core1 82
	/cluster0/fluke85/core2 82
	/cluster0/fluke85/core3 82
	/cluster0/fluke86 83
	/cluster0/fluke86/core0 83
	/cluster0/fluke86/core1 83
	/cluster0/fluke86/core2 83
	/cluster0/fluke86/core3 83
	/cluster0/fluke94 91
	/cluster0/fluke94/core0 91
	/cluster0/fluke94/core1 91
	/cluster0/fluke94/core2 91
	/cluster0/fluke95 92
	/cluster0/fluke95/core0 92
	/cluster0/fluke95/core1 92
	/cluster0/fluke95/core2 92
	/cluster0/fluke100 97
	/cluster0/fluke100/core3 97
	/cluster0/fluke102 99
	/cluster0/fluke102/core3 99
	EOF
    flux R encode -r 79-83 -c 0-3 -H fluke[82-86] > out2 &&
    flux R encode -r 91-92 -c 0-2 -H fluke[94-95] >> out2 &&
    flux R encode -r 97,99 -c 3 -H fluke[100,102] >> out2 &&
    cat out2 | flux R append > combined2.json &&
    cat combined2.json | flux ion-R encode > augmented2.json &&
    jq .scheduling augmented2.json > jgf2.json &&
    print_schema jgf2.json paths2 &&
    test_cmp expected2 paths2
'

test_expect_success 'fluxion-R: encoding nodelists with reverse mapping' '
    cat <<-EOF >expected3 &&
	/cluster0 null
	/cluster0/fluke102 79
	/cluster0/fluke102/core0 79
	/cluster0/fluke102/core1 79
	/cluster0/fluke102/core2 79
	/cluster0/fluke102/core3 79
	/cluster0/fluke101 80
	/cluster0/fluke101/core0 80
	/cluster0/fluke101/core1 80
	/cluster0/fluke101/core2 80
	/cluster0/fluke101/core3 80
	/cluster0/fluke100 81
	/cluster0/fluke100/core0 81
	/cluster0/fluke100/core1 81
	/cluster0/fluke100/core2 81
	/cluster0/fluke100/core3 81
	/cluster0/fluke99 82
	/cluster0/fluke99/core0 82
	/cluster0/fluke99/core1 82
	/cluster0/fluke99/core2 82
	/cluster0/fluke99/core3 82
	/cluster0/fluke98 83
	/cluster0/fluke98/core0 83
	/cluster0/fluke98/core1 83
	/cluster0/fluke98/core2 83
	/cluster0/fluke98/core3 83
	/cluster0/fluke90 91
	/cluster0/fluke90/core0 91
	/cluster0/fluke90/core1 91
	/cluster0/fluke90/core2 91
	/cluster0/fluke89 92
	/cluster0/fluke89/core0 92
	/cluster0/fluke89/core1 92
	/cluster0/fluke89/core2 92
	/cluster0/fluke84 97
	/cluster0/fluke84/core3 97
	/cluster0/fluke84/gpu1 97
	/cluster0/fluke82 99
	/cluster0/fluke82/core3 99
	/cluster0/fluke82/gpu1 99
	EOF
    flux R encode -r 79-83 -c 0-3 -H fluke[102,101,100,99,98] > out3 &&
    flux R encode -r 91-92 -c 0-2 -H fluke[90,89] >> out3 &&
    flux R encode -r 97,99 -c 3 -g 1 -H fluke[84,82] >> out3 &&
    cat out3 | flux R append > combined3.json &&
    cat combined3.json | flux ion-R encode > augmented3.json &&
    jq .scheduling augmented3.json > jgf3.json &&
    print_schema jgf3.json paths3 &&
    test_cmp expected3 paths3
'

test_expect_success 'fluxion-R: can detect multiple ranks' '
    flux R encode -r 2-3 -c 0-1 -H foo[1,4] > out4 &&
    flux R encode -r 91-92 -c 0-2 -H fluke[90,89] >> out4 &&
    cat out4 | flux R append > c4.json &&
    jq ".execution.R_lite[0].rank = \"2-3,92\"" c4.json > c4.error.json &&
    cat c4.error.json | flux ion-R encode 2>&1 | grep -i error
'

test_expect_success 'fluxion-R: can detect insufficient nodelist' '
    flux R encode -r 2-3 -c 0-1 -H foo[1,4] > out5.json &&
    jq ".execution.nodelist= \"foo1\"" out5.json > c5.error.json &&
    cat c5.error.json | flux ion-R encode 2>&1 | tee err5 | grep -i error
'

test_expect_success 'fluxion-R: encoding properties on heterogeneity works' '
    cat <<-EOF >expected6 &&
	/cluster0 null null
	/cluster0/foo2 0 {"arm-v9@core":""}
	/cluster0/foo2/core0 0 null
	/cluster0/foo2/core1 0 null
	/cluster0/foo2/gpu0 0 null
	/cluster0/foo2/gpu1 0 null
	/cluster0/foo3 2 {"arm-v9@core":"","amd-mi60@gpu":""}
	/cluster0/foo3/core0 2 null
	/cluster0/foo3/core1 2 null
	/cluster0/foo3/gpu0 2 null
	/cluster0/foo3/gpu1 2 null
	/cluster0/foo1 3 {"arm-v9@core":"","amd-mi60@gpu":""}
	/cluster0/foo1/core0 3 null
	/cluster0/foo1/core1 3 null
	/cluster0/foo1/gpu0 3 null
	/cluster0/foo1/gpu1 3 null
	/cluster0/foo4 1 {"arm-v8@core":""}
	/cluster0/foo4/core0 1 null
	EOF
    flux R encode -r 0 -c 0-1 -g 0-1 -p "arm-v9@core:0" -H foo2 > out6 &&
    flux R encode -r 1 -c 0 -H foo3 -p "arm-v8@core:1" >> out6 &&
    flux R encode -r 2-3 -c 0-1 -g 0-1 -p "arm-v9@core:2-3" \
	-p "amd-mi60@gpu:2-3" -H foo[1,4] >> out6 &&
    cat out6 | flux R append > combined6.json &&
    cat combined6.json | flux ion-R encode > augmented6.json &&
    jq .scheduling augmented6.json > jgf6.json &&
    print_schema2 jgf6.json paths6 &&
    test_cmp expected6 paths6
'

test_done
