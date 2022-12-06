from string import Template
import random

colors = [0, 31, 67, 104, 146, 183, 219, 255]
rect_template = Template("<rect width=\"$width\" height=\"$height\" x=\"$x\" y=\"$y\" fill=\"rgb($color,$color,$color)\"/>")

def gen_code(xcoord, ycoord, char_width, char_height, code_width):
    this_code = ""
    prev_idx = 0
    for disp in range(code_width):
        _xcoord = round(xcoord - disp*char_width, 2)
        _ycoord = ycoord
        if (disp==0):
            _color_idx = 7
        elif (disp==1): 
            _color_idx = 0
        else:
            while (_color_idx == prev_idx):
                _color_idx = random.randint(0,6)
            prev_idx = _color_idx
        _color = colors[_color_idx]

        this_char = rect_template.substitute(width = char_width,
                                height = char_height, 
                                x = _xcoord, 
                                y = _ycoord,
                                color = _color)
        this_code += this_char + "\n"

    return this_code


def gen_line(ycoord, char_width, char_height,  code_width, line_width):

    this_line = ""
    xcoord = 15
    word_length = code_width * char_width
    no_codes = int(line_width/word_length)

    for i in range(no_codes):
        _xcoord = round(xcoord - word_length*i, 2)
        this_code = gen_code(xcoord = _xcoord,
                            ycoord = ycoord,
                            char_width = char_width,
                            char_height = char_height,
                            code_width = code_width)
        
        this_line += (this_code + "\n")

    return this_line


def gen_page(filename, char_width, code_width, code_height, page_height, page_width):
    no_y_codes = int(page_height/code_height)

    with open(filename, "w") as f:
        for y_disp in range(no_y_codes, 0, -2):
            _ycoord = round(page_height - y_disp * code_height, 2)

            this_line = gen_line(ycoord=_ycoord,
                                char_width = char_width,
                                char_height = code_height,
                                code_width = code_width,
                                line_width = page_width
                                )
            
            f.write(this_line + "\n\n")


def main():

    filename = "samples"
    char_width = 0.4
    code_width = 6
    code_height = 0.4
    page_height = 16
    page_width = 16

    gen_page(filename, char_width, code_width, code_height, page_height, page_width)







if __name__ == "__main__":
    main()