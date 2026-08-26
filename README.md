## Abstract  
DeckCAD is an ambitious passion project with the goal to to satisfy my needs for a fast, fully offline and free to use Parametric 3D CAD software.
As a 3D printing hobbist I have used a variety of *Parametric 3D Design* platforms over the last ~9 Years, and every platform has left me desiring more (or less). 
CAD isn't my job, so I don't want to pay multiple hundreds for yearly licenses (I have paid for license out of necessity) that I only use occasionaly. 

## Design Philosophy
1) Local Performance - The application must runs localy, utilizing the hardware you paid for and never connecting to a server to process or compute any data.  
2) Efficiently intuitive - The application teaches you how to use it, I don't want to write a wiki.
3) Limited Inputs - Utilize the least amount of distinct input methods to perform actions. (Prefer multiple simple actions over one complex action - this will be important later).
4) Learn - Experimentation and learning new things is more important than getting to the finish zone quickly.

## Stack
| Concern | Library |
|---|---|
| Window & input | SDL3 |
| Graphics | WebGPU (Dawn) |
| UI | Dear ImGui |
| Viewport text | FreeType + msdfgen |
| Icons | nanosvg |

See [ARCHITECTURE.md](ARCHITECTURE.md) for the module layout and rendering conventions.

## Build
To build the project follow the instructions in [BUILD.md](BUILD.md).

## Contribution
This project will be kept as Open Source and Closed Commit.
- A friend of mine is joining me on this endeavor in a "I want to work on something, give me a task please" way. 