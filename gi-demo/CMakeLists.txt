
if(APPLE)
  set(CMAKE_XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/gi-demo-Bridging-Header.h")
  set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

set(
  SRCS
  app.cpp
  AppDelegate.swift
  AppOpenGLView.swift
  debug_draw.cpp
  gi-demo-Bridging-Header.h
  ViewController.swift
  vendor/tinyobjloader/tiny_obj_loader.cc
)

set(
  RESOURCES
  Base.lproj/Main.storyboard
)

add_executable(gi-demo MACOSX_BUNDLE ${SRCS} ${RESOURCES})
target_compile_features(gi-demo PRIVATE cxx_nullptr)
target_include_directories(gi-demo PRIVATE vendor/vectorial/include)

set_target_properties(gi-demo PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
  XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/gi-demo-Bridging-Header.h"
  INSTALL_RPATH "@loader_path/../Frameworks"
  RESOURCE ${RESOURCES}
)
