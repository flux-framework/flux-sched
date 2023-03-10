add_executable(expr_eval_test
  ${CMAKE_CURRENT_SOURCE_DIR}/expr_eval_test01.cpp
  )
target_link_libraries(expr_eval_test PRIVATE
  libtap
  resource
  )

add_test(NAME expr_eval_test COMMAND expr_eval_test)
