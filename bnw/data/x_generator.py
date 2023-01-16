import svgwrite
import string
import random 
from svgwrite import cm, mm


class XCodeGenerator: 
    def __init__(self, start_pattern=None, end_pattern=None, code_space=None, code_points=None):

        """
        parameters:
            start_pattern: Enum("A1", "A2", "B1", "B2") _-> Describes what the declarative region should look like
                "A1" ->  Ascending full declaration -- all code points appear in the prologue in ascending order
                "A2" -> Ascending Alternate declaration -- The first code point appears, but following that, only alternating code points are drawn
                "B1" -> Descending full declaration -- similar to "A1" but code points appear in descending order instead
                "B2" -> Descending Alternate declartion -- similar to "A2" but code points appear in descending order instead

            end_pattern: Enum("Y", "Z")
                "Y" -> code ends with a transistion from the highest code point to the lowest code point
                "Z" -> code ends with a transition from the lowest code point to the highest code point                        
            

            code_space: int -> Number of code-points  **must be an odd integer greater than 3

            code_points: Iterable of int -> List of code points (shades of gray in 8-bit black/white). Should be strictly ascending. 

        """

        if start_pattern is None:
            start_pattern = "B2"
        if type(start_pattern) != str or start_pattern.upper() not in ("A1", "A2", "B1", "B2"):
            raise Exception ("Invalid argument to Initializer")
        self.start_pattern = start_pattern

        if end_pattern is None:
            end_pattern = "Y"
        if type(end_pattern) != str or end_pattern.upper() not in ("Y", "Z"):
            raise Exception ("Invalid argument to Initializer")
        self.end_pattern = end_pattern

        if code_space is None and code_points is None:
            raise Exception ("Neither of parameters 'code_space' nore'code_points' was passed")


        if code_space is None:
            if not (len(code_points) % 2):
                raise Exception ("There should be an odd number of code_points")
            
            if len(code_points) < 3:
                raise Exception ("Number of code points should be >= 3")
            
            self.code_space = len(code_points)

        else:
            self.code_space = int(code_space)
        
        if code_points is None:
            self.code_points = [int(255 * a/(self.code_space-1)) for a in range(self.code_space)]
            self.code_points.reverse()


        elif self.code_space != len(code_points):
            raise Exception ("Mismatch between code_space and length of code_points")

        self.code_letters = [char for char in string.ascii_uppercase[:self.code_space]]


    def create_random_code(self, length):
        
        """
        Create a random string of specified length that fulfills the requirements of x-codes. The generated string
        does not include the declaration and termination sections. These are added with the wrap code method

        parameters:
            length: int -> desired length of generated x-code
        
        returns:
            random_code: string
        
        """

        random_code = [""] * length

        if self.start_pattern[0] == "A":
            prev_idx = self.code_space - 1
        elif self.start_pattern[0] == "B":
            prev_idx = 0
        next_idx = prev_idx
        for i in range(length):
            if self.end_pattern=="Z":
                while prev_idx==next_idx or (prev_idx==0 and next_idx==self.code_space-1):
                    next_idx = (prev_idx + random.randint(1, self.code_space-1)) % (self.code_space)
            elif self.end_pattern=="Y":
                while prev_idx==next_idx or (prev_idx==self.code_space-1 and next_idx==0):
                    next_idx = (prev_idx + random.randint(1, self.code_space-1)) % (self.code_space)

            random_code[i] = self.code_letters[next_idx]
            prev_idx = next_idx
        
        return "".join(random_code)
        

    def wrap_code(self, code: str):
        """
            Wrap a code with declaration and termination sections.

            parameters:
                code: string
            
            returns:
                wrapped_code: string
        
        """

        code_length = len(code)
        decl_length = self.code_space if self.start_pattern in ("A1", "B1") else self.code_space//2 + 1
        epi_length = 2
        true_length = code_length + decl_length + epi_length

        thiscode = [""] * true_length

        for i in range(0, decl_length):
            idx = i 
            if self.start_pattern[1] == "2":
                idx =  2 * idx
            if self.start_pattern[0] == "A": 
                thiscode[i] = self.code_letters[idx]
            elif self.start_pattern[0] == "B":
                thiscode[i] = self.code_letters[self.code_space-1-idx]
        
        # if self.start_pattern[0] == "A":
        #     prev_idx = self.code_space-1
        # elif self.start_pattern[0] == "B":
        #     prev_idx = 0

        for i in range(code_length):
            thiscode[decl_length+i] = code[i]
        

        if self.end_pattern == "Y":
            thiscode[decl_length + code_length] = self.code_letters[-1]
            thiscode[decl_length + code_length + 1] = self.code_letters[0]
        
        elif self.end_pattern == "Z": 
            thiscode[decl_length + code_length] = self.code_letters[0]
            thiscode[decl_length + code_length + 1] = self.code_letters[-1]


        return "".join(thiscode)

    
    def draw_code(self, drawing, code, coord, bar_dim, unit):

        """
        Draw a specified code in svg at a specified location (top left of code)

        parameters:

            drawing: -> An svgwrite drawing object
            code: str -> string representing an x-code
            coord: tuple(x-coord, y-coord) -> code will be drawn with it's top left at this coord
            bar_dim: the dimensions of each letter(bar) within the code


        returns:
            None

        side-effect:
            modification to drawing object 

        """
 
        colormap = dict(zip(self.code_letters, self.code_points))
        # print(colormap)

        for letter in code:

            this_fill = colormap[letter]
            this_fill = f'rgb({this_fill}, {this_fill}, {this_fill})'
            drawing.add(drawing.rect(coord, size=(bar_dim[0]*unit, bar_dim[1]*unit), fill=this_fill))
            coord = (coord[0]+bar_dim[0], coord[1])
        

        return coord


        
        
    def generate_random_page (self, page_dim, margin,  bar_dim, code_length, filename, unit=mm):

        bar_x_dim, bar_y_dim = bar_dim
        page_x_dim, page_y_dim = page_dim
        x_margin, y_margin = margin
        dwg = svgwrite.Drawing(filename=filename , debug=True)

        x_start = x_margin
        x_end = page_x_dim - x_margin

        y_start = y_margin
        y_end = page_y_dim - y_margin


        curr_coord = (x_start, y_start)

        dwg.add(dwg.rect((0,0), size=('100%', '100%'), fill='rgb(255,255,255)'))

        while curr_coord[1] < y_end:

            # print("here")
            while curr_coord[0] < x_end:

                rand_code = self.create_random_code(code_length)
                rand_code = self.wrap_code(rand_code)

                curr_coord = self.draw_code(drawing=dwg, coord=curr_coord, code=rand_code, bar_dim=bar_dim, unit=unit)
                
                dwg.add(dwg.rect(curr_coord, size=(bar_dim[0]*unit, 3*bar_dim[1]*unit), fill='rgb(255,255,255)'))

                curr_coord = (curr_coord[0] + bar_x_dim*3, curr_coord[1])

            # update y_coord to start of next possible line
            curr_coord = (x_start, curr_coord[1] + bar_y_dim)
            dwg.add(dwg.rect(curr_coord, size=('100%', bar_y_dim*unit), fill='rgb(255,255,255)'))
            curr_coord = (x_start, curr_coord[1] + bar_y_dim)

        dwg.save()




if __name__ == "__main__":

    a4_dim = (210, 297)
    page_margin = (10, 20)
    bar_dim = (1, 2.5)


    for start_pattern in ("A1", "B1", "B2", "B1"):
        for end_pattern in ("Y", "Z"):
            for code_space in (3, 5): 
                file_name = f'{start_pattern}-{end_pattern}-{code_space}.svg'
                thisGenerator = XCodeGenerator(
                    start_pattern=start_pattern,
                    end_pattern=end_pattern,
                    code_space=code_space)
                
                thisGenerator.generate_random_page(
                    page_dim = a4_dim,
                    margin = page_margin,
                    bar_dim = bar_dim,
                    code_length = 5,
                    filename=file_name
                )



    # thisGenerator = XCodeGenerator("B2", "Z", 3)
    # print(thisGenerator.code_points)
    # print(thisGenerator.wrap_code("ABC"))
    # print(thisGenerator.create_random_code(20))
    
    # randomc = thisGenerator.create_random_code(4)
    # randomc = thisGenerator.wrap_code(randomc)
    # print(randomc)

    # a4_dim = (210, 297)
    # page_margin = (10, 20)
    # bar_dim = (1, 2.5)

    # print(mm)

    # print (type(1 * mm))
    # thisGenerator.generate_random_page(page_dim=a4_dim, margin=page_margin, bar_dim=bar_dim, code_length=5, filename="try.svg")


    

