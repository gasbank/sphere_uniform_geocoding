# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(sphere_uniform_geocoding_library VERSION 0.0.1 LANGUAGES C)

add_library(sphere_uniform_geocoding SHARED
  "sphere_uniform_geocoding.c"
)

set_target_properties(sphere_uniform_geocoding PROPERTIES
  PUBLIC_HEADER sphere_uniform_geocoding.h
  OUTPUT_NAME "sphere_uniform_geocoding"
)

target_compile_definitions(sphere_uniform_geocoding PUBLIC DART_SHARED_LIB)
