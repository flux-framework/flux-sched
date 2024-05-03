add_executable(expr_eval_test
        expr_eval_test01.cpp
  )
target_link_libraries(expr_eval_test PRIVATE
  libtap
  resource
  )

flux_add_test(NAME expr_eval_test COMMAND expr_eval_test)
