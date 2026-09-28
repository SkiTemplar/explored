# Tests del host: compila los modelos puros de Source/Explored contra el shim de Core
# y ejecuta los tests de tests/*.cpp. Ver README.md.
cmake_minimum_required(VERSION 3.20)
project(ExploredHostTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/Explored)

# Modelos puros: solo dependen de CoreMinimal (sin UObject). Añadir aquí los nuevos.
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/pure_sources.txt PURE_LIST ENCODING UTF-8 REGEX "^[^#].*")
set(PURE_SOURCES)
foreach(F ${PURE_LIST})
	list(APPEND PURE_SOURCES ${SRC}/${F})
endforeach()

# Automation Specs de UE que también corren aquí (misma fuente de verdad en ambos lados).
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/pure_specs.txt SPEC_LIST ENCODING UTF-8 REGEX "^[^#].*")
foreach(F ${SPEC_LIST})
	list(APPEND PURE_SOURCES ${SRC}/${F})
endforeach()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

add_executable(explored_host_tests main.cpp ${PURE_SOURCES} ${TEST_SOURCES})
target_include_directories(explored_host_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/shim ${CMAKE_CURRENT_SOURCE_DIR} ${SRC})
target_compile_options(explored_host_tests PRIVATE -Wall -Wextra -Wno-unused-parameter
	-Wno-missing-field-initializers -Wno-sign-compare -Werror=return-type -Werror=uninitialized)

# HOST_TESTS_SANITIZE=ON: AddressSanitizer + UndefinedBehaviorSanitizer (lecturas fuera de
# rango, desbordamientos con signo, conversiones float→int fuera de rango...).
option(HOST_TESTS_SANITIZE "Compilar con ASan y UBSan" OFF)
if(HOST_TESTS_SANITIZE)
	target_compile_options(explored_host_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
		-fno-sanitize-recover=undefined -fsanitize=float-cast-overflow)
	target_link_options(explored_host_tests PRIVATE -fsanitize=address,undefined)
endif()

# HOST_TESTS_FASTMATH=ON: matemáticas rápidas, como el editor (MSVC /fp:fast). El compilador
# supone que no hay NaN ni infinitos, así que `!(x > 0)` deja pasar un NaN; solo las
# comprobaciones por bits (FMath::IsFinite/IsNaN) los siguen viendo. Sirve para cazar los
# sitios que confían en la semántica IEEE de las comparaciones con NaN.
option(HOST_TESTS_FASTMATH "Compilar con matemáticas rápidas (-ffast-math)" OFF)
if(HOST_TESTS_FASTMATH)
	target_compile_options(explored_host_tests PRIVATE -ffast-math)
endif()
