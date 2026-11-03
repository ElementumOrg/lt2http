# Helper macros for building lt2http
include(FeatureSummary)

# Issues option() and add_feature_info() in a single call.
# Synopsis: feature_option(<name> <description> <default_value>)
macro(feature_option _name _description _default)
    option(${_name} "${_description}" ${_default})
    add_feature_info(${_name} ${_name} "${_description}")
endmacro()

# Issues a cache set() and add_feature_info() in a single call.
# Synopsis: feature(<name> <type> <enabled> <description> <default_value>)
macro(feature _name _type _enabled _description _default)
    set(${_name} "${_default}" CACHE ${_type} "${_description}")
    add_feature_info(${_name} ${_enabled} "${_description}")
endmacro()
