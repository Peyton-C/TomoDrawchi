class ColourRange:
    EXT = False
    COLOURS = []

    @staticmethod
    def hex_to_rgb(value):
        if isinstance(value, tuple):
            return tuple(ColourRange.hex_to_rgb(h) for h in value)
        value = value.lstrip("#")
        return tuple(int(value[i : i + 2], 16) for i in (0, 2, 4))
    
    @classmethod
    def flat_colours(cls):
        for entry in cls.COLOURS:
            if isinstance(entry, tuple):
                yield from entry
            else:
                yield entry
    
    @classmethod
    def get_position(cls, hex_str):
        """Returns (x, y) where x is tuple index (col) and y is list index (row)."""
        hex_str = hex_str.upper().strip()
        for y, entry in enumerate(cls.COLOURS):
            if isinstance(entry, tuple):
                for x, h in enumerate(entry):
                    if h.upper() == hex_str:
                        return (x, y)
            else:
                if entry.upper() == hex_str:
                    return (0, y)
        return None

class BASIC_RANGE(ColourRange):
    EXT = False
    COLOURS = [
        ("#000000"),
        ("#9B9B98"),
        ("#FEFEFE"),
        ("#F60201"),
        ("#09FC04"),
        ("#0000F4"),
        ("#12FDF9"),
        ("#F704BA"),
        ("#FBFE06")
    ]


class EXT_RANGE(ColourRange):
    EXT = True
    COLOURS = [
        ("#FEFEFE", "#EDEFF3", "#EEF0F4", "#EEF7FA", "#EEFAF0", "#EDF3ED", "#F1FAEC", "#F0F2E3", "#F7F1EC", "#F6F0ED", "#F7EBD9", "#F60201"),
        ("#EAEAEA", "#CAC8E4", "#C6CBE1", "#C6E6F8", "#C7F0D6", "#C6D8C6", "#D9ECC8", "#F8F8C6", "#F7D6C7", "#EAC8C7", "#E0CFAE", "#FBFE06"),
        ("#D4D4D2", "#A192D0", "#929DCE", "#92D5F7", "#91E4B6", "#90BB91", "#BADF92", "#F5F392", "#F4B490", "#DB9691", "#C6A976", "#09FC04"),
        ("#B8BBB8", "#5F02BA", "#004AB6", "#0FC0F4", "#04B979", "#039416", "#90D118", "#F4EE03", "#F08402", "#CE2800", "#8B610E", "#12FDF9"),
        ("#9B9B98", "#4F019F", "#003E9D", "#08A5D0", "#06915F", "#047E0E", "#7CB20E", "#D1CD03", "#CF7102", "#AE2300", "#724200", "#0000F4"),
        ("#717171", "#3D027C", "#01317A", "#08A5D0", "#06915F", "#02630D", "#618D0F", "#A5A204", "#A25901", "#8A1601", "#58380D", "#8202F4"),
        ("#000000", "#1D0144", "#001544", "#01485C", "#06915F", "#003700", "#345001", "#5D5D02", "#5B2E00", "#4B0E00", "#30230D", "#F704BA")
    ]