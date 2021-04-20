/**
 * Copyright (C) 2021 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "ResourcesSmartNic.h"

const std::string & getSmartNicSchema()
{
  static std::string SmartNicSchema = R"~~~~(
{
  "$schema": "https://json-schema.org/draft/2019-09/schema",
  "title": "SmartNic JSON Schema",
  "description": "This Schema checks the full content of the SmartNic JSON syntax",
  "type": "object",
  "required": [],
  "additionalProperties": false,
  "properties": {
    "schema_version": {
      "type": "object",
      "required": [
        "major",
        "minor",
        "patch"
      ],
      "additionalProperties": false,
      "properties": {
        "major": {
          "type": "integer",
          "minimum": 0
        },
        "minor": {
          "type": "integer",
          "minimum": 0
        },
        "patch": {
          "type": "integer",
          "minimum": 0
        }
      }
    },
    "extensions": {
      "type": "array",
      "items": {
        "type": "object",
        "required": [
          "instance_name"
        ],
        "additionalProperties": false,
        "properties": {
          "version_info": {
            "type": "object",
            "required": [
              "uuid",
              "minor",
              "patch"
            ],
            "additionalProperties": false,
            "properties": {
              "uuid": {
                "type": "string",
                "extendedType": "hex-encoded",
                "minLength": 32,
                "maxLength": 32,
                "pattern": "^([0-9a-fA-F]{2})+$"
              },
              "minor": {
                "type": "integer",
                "minimum": 0
              },
              "patch": {
                "type": "integer",
                "minimum": 0
              }
            }
          },
          "instance_name": {
            "type": "string"
          },
          "info": {
            "type": "array",
            "items": {
              "type": "object",
              "additionalProperties": false,
              "properties": {
                "title": {
                  "type": "string"
                },
                "creator": {
                  "type": "string"
                },
                "subject": {
                  "type": "string"
                },
                "description": {
                  "type": "string"
                },
                "publisher": {
                  "type": "string"
                },
                "date": {
                  "type": "string"
                },
                "type": {
                  "type": "string"
                },
                "format": {
                  "type": "string"
                },
                "identifier": {
                  "type": "string"
                },
                "source": {
                  "type": "string"
                },
                "language": {
                  "type": "string"
                },
                "relation": {
                  "type": "string"
                },
                "coverage": {
                  "type": "string"
                },
                "rights": {
                  "type": "string"
                }
              }
            }
          },
          "features_enum": {
            "type": "array",
            "extendedType": "enum-encoded",
            "items": {
              "type": "string",
              "enum": [
                "Feature1",
                "Feature2"
              ]
            }
          },
          "features": {
            "type": "string",
            "extendedType": "hex-encoded"
          },
          "cam_instances": {
            "type": "array",
            "items": {
              "type": "object",
              "oneOf": [
                {
                  "required": [
                    "config"
                  ]
                },
                {
                  "required": [
                    "config_file"
                  ]
                }
              ],
              "required": [
                "name"
              ],
              "additionalProperties": false,
              "properties": {
                "name": {
                  "type": "string"
                },
                "id": {
                  "type": "integer",
                  "minimum": 0
                },
                "config": {
                  "type": "string",
                  "extendedType": "hex-encoded",
                  "pattern": "^([0-9a-fA-F]{2})+$"
                },
                "config_file": {
                  "type": "string",
                  "extendedType": "file-image"
                },
                "base_address": {
                  "type": "integer",
                  "minimum": 0
                },
                "driver_index": {
                  "type": "integer",
                  "minimum": 0
                }
              }
            }
          },
          "address_mapping": {
            "type": "object",
            "required": [
              "offset",
              "aperture_size_bytes"
            ],
            "additionalProperties": false,
            "properties": {
              "offset": {
                "type": "integer",
                "minimum": 0
              },
              "aperture_size_bytes": {
                "type": "integer",
                "minimum": 0
              }
            }
          },
          "setup": {
            "type": "object",
            "oneOf": [
              {
                "required": [
                  "ebpf"
                ]
              },
              {
                "required": [
                  "ebpf_file"
                ]
              }
            ],
            "additionalProperties": false,
            "properties": {
              "ebpf": {
                "type": "string",
                "extendedType": "hex-encoded",
                "pattern": "^([0-9a-fA-F]{2})+$"
              },
              "ebpf_file": {
                "type": "string",
                "extendedType": "file-image"
              }
            }
          },
          "background_proc": {
            "type": "object",
            "oneOf": [
              {
                "required": [
                  "ebpf"
                ]
              },
              {
                "required": [
                  "ebpf_file"
                ]
              }
            ],
            "additionalProperties": false,
            "properties": {
              "ebpf": {
                "type": "string",
                "extendedType": "hex-encoded",
                "pattern": "^([0-9a-fA-F]{2})+$"
              },
              "ebpf_file": {
                "type": "string",
                "extendedType": "file-image"
              }
            }
          },
          "tear_down": {
            "type": "object",
            "oneOf": [
              {
                "required": [
                  "ebpf"
                ]
              },
              {
                "required": [
                  "ebpf_file"
                ]
              }
            ],
            "additionalProperties": false,
            "properties": {
              "ebpf": {
                "type": "string",
                "extendedType": "hex-encoded",
                "pattern": "^([0-9a-fA-F]{2})+$"
              },
              "ebpf_file": {
                "type": "string",
                "extendedType": "file-image"
              }
            }
          },
          "messages": {
            "type": "array",
            "items": {
              "type": "object",
              "oneOf": [
                {
                  "required": [
                    "ebpf"
                  ]
                },
                {
                  "required": [
                    "ebpf_file"
                  ]
                }
              ],
              "required": [
                "id",
                "name",
                "param_size_bytes"
              ],
              "additionalProperties": false,
              "properties": {
                "id": {
                  "type": "integer",
                  "minimum": 0
                },
                "name": {
                  "type": "string"
                },
                "param_size_bytes": {
                  "type": "integer",
                  "minimum": 0
                },
                "ebpf": {
                  "type": "string",
                  "extendedType": "hex-encoded",
                  "pattern": "^([0-9a-fA-F]{2})+$"
                },
                "ebpf_file": {
                  "type": "string",
                  "extendedType": "file-image"
                }
              }
            }
          },
          "resource_classes": {
            "type": "array",
            "items": {
              "type": "object",
              "oneOf": [
                {
                  "required": [
                    "dtor"
                  ]
                },
                {
                  "required": [
                    "dtor_file"
                  ]
                }
              ],
              "required": [
                "name",
                "description",
                "max_count",
                "memory_size_bytes"
              ],
              "additionalProperties": false,
              "properties": {
                "name": {
                  "type": "string"
                },
                "description": {
                  "type": "string"
                },
                "max_count": {
                  "type": "integer",
                  "exclusiveMinimum": 0
                },
                "memory_size_bytes": {
                  "type": "integer",
                  "minimum": 0
                },
                "dtor": {
                  "type": "string",
                  "extendedType": "hex-encoded",
                  "pattern": "^([0-9a-fA-F]{2})+$"
                },
                "dtor_file": {
                  "type": "string",
                  "extendedType": "file-image"
                }
              }
            }
          },
          "global_memory_size_bytes": {
            "type": "integer",
            "minimum": 0,
            "maximum": 4096
          },
          "per_handle_memory_size_bytes": {
            "type": "integer",
            "minimum": 0,
            "maximum": 256
          }
        }
      }
    },
    "softhubs": {
      "type": "array",
      "items": {
        "type": "object",
        "oneOf": [
          {
            "required": [
              "id"
            ]
          },
          {
            "required": [
              "id_enum"
            ]
          }
        ],
        "properties": {
          "version": {
            "type": "integer"
          },
          "id": {
            "type": "integer"
          },
          "id_enum": {
            "type": "string",
            "extendedType": "enum-encoded",
            "enum": [
              "VNT2P",
              "P2HMAE",
              "MAE2P",
              "P2VNR"
            ]
          }
        }
      }
    },
    "cam_drivers": {
      "type": "array",
      "items": {
        "type": "object",
        "required": [
          "compatible_version"
        ],
        "additionalProperties": false,
        "properties": {
          "compatible_version": {
            "type": "integer"
          },
          "driver": {
            "type": "string",
            "extendedType": "hex-encoded",
            "pattern": "^([0-9a-fA-F]{2})+$"
          },
          "driver_file": {
            "type": "string",
            "extendedType": "file-image"
          },
          "signature": {
            "type": "string",
            "extendedType": "hex-encoded",
            "pattern": "^([0-9a-fA-F]{2})+$"
          },
          "signature_file": {
            "type": "string",
            "extendedType": "file-image"
          }
        }
      }
    }
  }
}
)~~~~";

  return SmartNicSchema;
}

