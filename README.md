# gi-demo

I read this blog post, http://copypastepixel.blogspot.com/2017/04/real-time-global-illumination.html, and decided to see how far I could get trying to replicate the results by starting from scratch. The code is intentionally a mess. :-p

## TODO
- add materials to the elements of the cornell box
- create a static lightmap (just for fun!)
  - for each model:
    - extract triangles from the mesh
    - pack them into the texture (http://thomasdiewald.com/blog/?p=2099)
