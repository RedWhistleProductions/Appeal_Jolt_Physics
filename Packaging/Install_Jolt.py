"""Install pinned Jolt 5.6.0 as a portable static dependency (Python, CMake, C++17)."""
from pathlib import Path
import hashlib,json,os,platform,shutil,subprocess,tarfile,urllib.request
base=Path(__file__).resolve().parents[1]/'.deps/jolt';base.mkdir(parents=True,exist_ok=True)
commit='e77f175595e64cb44218cc9d9d56fc365ad0e36a'
sha='1f32328fb763135de10a244568d6ccb2ed9b1e6593fafe6dc6db5b2719d330bd'
url=f'https://codeload.github.com/jrouwe/JoltPhysics/tar.gz/{commit}'
archive=base/'jolt.tar.gz'
if not archive.exists():archive.write_bytes(urllib.request.urlopen(url,timeout=90).read())
if hashlib.sha256(archive.read_bytes()).hexdigest()!=sha:raise RuntimeError('Jolt archive checksum mismatch')
source=base/'source';source.mkdir(exist_ok=True)
manifest=base/'SOURCE.json'
installed=json.loads(manifest.read_text()) if manifest.exists() else {}
if installed.get('commit')!=commit or not (source/'Jolt/Jolt.h').exists():
 with tarfile.open(archive) as tar:
  for item in tar.getmembers():
   if not(item.isfile() or item.isdir()):continue
   parts=Path(item.name).parts[1:]
   if not parts:continue
   target=source.joinpath(*parts).resolve()
   if not target.is_relative_to(source.resolve()):raise RuntimeError('Unsafe archive path')
   if item.isdir():target.mkdir(parents=True,exist_ok=True)
   else:target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(tar.extractfile(item).read())
(base/'SOURCE.json').write_text(json.dumps(dict(tag='v5.6.0',commit=commit,url=url,sha256=sha),indent=2))
system=platform.system().lower();build=base/'build'/system
args=['cmake','-S',str(source/'Build'),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_POSITION_INDEPENDENT_CODE=ON']
if os.name=='nt':
 compiler=shutil.which('g++');make=shutil.which('make')
 if not compiler or not make:raise RuntimeError('Put MinGW g++ and make on PATH')
 args+=['-G','MinGW Makefiles','-DCMAKE_MAKE_PROGRAM='+make,'-DCMAKE_CXX_COMPILER='+compiler,
        '-DCMAKE_CXX_COMPILER_FORCED=TRUE','-DCMAKE_CXX_COMPILER_ID_RUN=TRUE','-DCMAKE_CXX_COMPILER_ID=GNU','-DCMAKE_SIZEOF_VOID_P=8']
for name in ['TARGET_HELLO_WORLD','TARGET_PERFORMANCE_TEST','TARGET_UNIT_TESTS','TARGET_SAMPLES','TARGET_VIEWER',
 'JPH_USE_DX12','JPH_USE_VK','JPH_USE_MTL','JPH_USE_CPU_COMPUTE','INTERPROCEDURAL_OPTIMIZATION',
 'GENERATE_DEBUG_SYMBOLS','DEBUG_RENDERER_IN_DEBUG_AND_RELEASE','PROFILER_IN_DEBUG_AND_RELEASE','ENABLE_ALL_WARNINGS',
 'USE_AVX','USE_AVX2','USE_FMADD','USE_LZCNT','USE_TZCNT','USE_F16C','USE_SSE4_1','USE_SSE4_2']:args+=['-D'+name+'=OFF']
subprocess.run(args,check=True)
subprocess.run(['cmake','--build',str(build),'--target','Jolt','--parallel','4'],check=True)
print('Jolt installed:',build)
