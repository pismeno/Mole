import enum

class AssetType(enum.IntEnum):
    UNKNOWN = 0
    TEXT = 1
    GENERIC_BINARY = 2
    VERTEX_SHADER = 3
    FRAGMENT_SHADER = 4
    COMPUTE_SHADER = 5
    GEOMETRY_SHADER = 6
    TESS_CONTROL_SHADER = 7
    TESS_EVALUATION_SHADER = 8
    IMAGE = 9
    SCENE = 10
    MATERIAL = 11

    @classmethod
    def _missing_(cls, value):
        # Handle numeric string ("1", "2")
        if isinstance(value, str):
            if value.isdigit():
                val_int = int(value)
                if val_int in cls._value2member_map_:
                    return cls(val_int)

            # Handle uppercase name match ("TEXT", "IMAGE")
            upper_val = value.upper()
            if upper_val in cls._member_map_:
                return cls[upper_val]

        return cls.UNKNOWN
