import re
import sys
import os

def parse_map(filename, limit=20):
    if not os.path.exists(filename):
        print(f"Ошибка: Файл '{filename}' не найден.")
        return

    symbols = []
    
    # Улучшенная регулярка.
    # Ловит строки вида: .section  address  size  file
    # Группы:
    # 1: Имя секции (например .text.SystemInit или .bss.buffer)
    # 2: Размер (например 0x48)
    # 3: Имя объектного файла (например main.o)
    pattern = re.compile(r'^\s+(\.\S+)\s+0x[0-9a-fA-F]+\s+(0x[0-9a-fA-F]+)\s+(.+)$')
    
    total_size = 0

    try:
        with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                # Пропускаем пустые строки
                if not line.strip(): continue

                match = pattern.match(line)
                if match:
                    sec_name, size_hex, obj_file = match.groups()
                    try:
                        size = int(size_hex, 16)
                        # Отсеиваем нулевые размеры и служебные секции
                        if size > 0 and obj_file.strip() != "" and not sec_name.startswith(('.debug', '.comment', '.ARM.attributes')):
                            # Определяем тип памяти грубо по имени секции
                            mem_type = "RAM" if any(x in sec_name for x in ['.bss', '.data', 'stack', 'heap']) else "FLASH"
                            
                            symbols.append({
                                'size': size,
                                'name': sec_name,
                                'file': obj_file.strip(),
                                'type': mem_type
                            })
                            total_size += size
                    except ValueError:
                        continue
    except Exception as e:
        print(f"Ошибка при чтении файла: {e}")
        return

    # Сортируем: от большего к меньшему
    symbols.sort(key=lambda x: x['size'], reverse=True)
    
    # Вывод таблицы
    print(f"{'BYTES':>8} | {'TYPE':<5} | {'FILE':<25} | {'SECTION/SYMBOL'}")
    print("-" * 85)
    
    for item in symbols[:limit]:
        # Обрезаем имя файла, если оно слишком длинное, для красоты
        file_display = (item['file'][:22] + '..') if len(item['file']) > 250 else item['file']
        print(f"{item['size']:>8} | {item['type']:<5} | {file_display:<25} | {item['name']}")

    print("-" * 85)
    print(f"Показано топ-{limit} символов.")

if __name__ == "__main__":
    # Проверка аргументов командной строки
    if len(sys.argv) < 2:
        print("Использование: python map_parser.py <path_to_map_file> [limit]")
        print("Пример: python map_parser.py firmware.map 15")
    else:
        map_file = sys.argv[1]
        limit_count = int(sys.argv[2]) if len(sys.argv) > 2 else 20
        parse_map(map_file, limit_count)

