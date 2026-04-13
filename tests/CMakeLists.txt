add_executable(test_delay_line
  test_delay_line.cpp
)
target_link_libraries(test_delay_line PRIVATE orbit_delay_core)
add_test(NAME test_delay_line COMMAND test_delay_line)

add_executable(test_filters
  test_filters.cpp
)
target_link_libraries(test_filters PRIVATE orbit_delay_core)
add_test(NAME test_filters COMMAND test_filters)

add_executable(test_orbit_delay
  test_orbit_delay.cpp
)
target_link_libraries(test_orbit_delay PRIVATE orbit_delay_core)
add_test(NAME test_orbit_delay COMMAND test_orbit_delay)

add_executable(render_golden
  render_golden.cpp
)
target_link_libraries(render_golden PRIVATE orbit_delay_core)
add_test(NAME render_golden COMMAND render_golden)
