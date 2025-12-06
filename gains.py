def generate_gain_table():
    # Значения аттенюации (дБ) для индексов
    # LNAS [11]: 0=MaxAtten(19dB), 3=MinAtten(0dB)
    LNAS = [19, 16, 11, 0] 
    # LNA [111]: 0=MaxAtten(24dB), 7=MinAtten(0dB)
    LNA  = [24, 19, 14, 9, 6, 4, 2, 0] 
    # MIX [11]: 0=MaxAtten(8dB), 3=MinAtten(0dB)
    MIX  = [8, 6, 3, 0]  
    # PGA [111]: 0=MaxAtten(33dB), 7=MinAtten(0dB)
    PGA  = [33, 27, 21, 15, 9, 6, 3, 0] 

    candidates = []
    # Генерация всех 1024 комбинаций
    for lnas_i, lnas_v in enumerate(LNAS):
        for lna_i, lna_v in enumerate(LNA):
            for mix_i, mix_v in enumerate(MIX):
                for pga_i, pga_v in enumerate(PGA):
                    total_atten = lnas_v + lna_v + mix_v + pga_v
                    reg_val = (lnas_i << 8) | (lna_i << 5) | (mix_i << 3) | pga_i
                    
                    # RF Score: чем выше, тем лучше для чувствительности.
                    # Приоритет: держать LNAS (старшие биты) и LNA как можно дольше.
                    rf_score = (lnas_i * 10) + lna_i 
                    
                    candidates.append({
                        'atten': total_atten,
                        'reg': reg_val,
                        'rf_score': rf_score
                    })

    # Целевая кривая: 31 шаг от 0 до 84 дБ
    steps = 31
    max_atten = 84
    table = []
    
    # print(f"{'Idx':<3} | {'Target':<6} | {'Actual':<6} | {'Hex':<6} | {'Logic Check'}")
    # print("-" * 50)

    for i in range(steps):
        target = i * (max_atten / (steps - 1))
        
        # Фильтрация: ищем кандидата, который ближе всего к целевой аттенюации.
        # При равенстве (или близости) выбираем того, у кого выше RF Score (LNA gain).
        best = min(candidates, key=lambda x: (abs(x['atten'] - target), -x['rf_score']))
        
        table.append((best['reg'],best['atten']))
        # print(f"{i:<3} | {target:<6.1f} | {best['atten']:<6} | 0x{best['reg']:04X} | RF_Score: {best['rf_score']}")

    return table

t = generate_gain_table()
for v,g in t:
    print("{0x%x, %d},"%(v,g))

