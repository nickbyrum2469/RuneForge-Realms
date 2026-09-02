from pathlib import Path

p = Path('src/render/scene/VoxelCharacterBuilder.cpp')
text = p.read_text()
old = '-body.up'
count = text.count(old)
if count != 4:
    raise SystemExit(f'expected four unary body.up uses, found {count}')
p.write_text(text.replace(old, 'body.up * -1.0f'))
print('character compile correction applied')
