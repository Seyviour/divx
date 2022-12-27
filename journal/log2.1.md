# Log 2.1

I think some modifications need to be made to how x-codes are defined:

1. Rather than mark the start and end with one of the letters (w-white as stated in the previous log), we could mark starts and ends with specific transitions between letters. This is a less wasteful use of the encoding space. I think the transition from b-black to w-white is a good candidate for this role.
2. It might be a good idea to define the letters of an x-code within each x-code. This might help adaptive decoding and help us support more print surfaces and materials. In addition to that, this would presumably make the x-code printing process less demanding on printer accuracy/consistency. For a 5-point x-code (with letters A,B,C,D,E), the x-code could run thus: ACE*****EA. The B and D code points are implicitly defined. The shortest possible x-code scheme if we adopt this would be a 3-point x-code (essentially binary), with more dense x-code schemes possible. I find this scheme attractive. 

I will flesh these ideas out in Log 3; I've stated them here because I'll (probably) be incorporating them during the work that goes on before log3. 