import os
import random


def generate_mock_csv(symbol, filename, scale, base_price, num_ticks=10000):
    print(f"[+] Generating synthetic ticks for {symbol} -> {filename}")
    
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    
    with open(filename, "w") as f:
        current_price = base_price
        for i in range(num_ticks):
            ms_offset = 100000 + (i * 12)
            timestamp_str = f"20260101 1704{ms_offset:05d}"
            
            current_price += random.uniform(-0.00015, 0.00015)
            bid = round(current_price, scale)
            
            is_spike = (i % 500 == 0)
            spread = random.uniform(0.0002, 0.0004) if is_spike else random.uniform(0.00002, 0.00008)
            ask = round(bid + spread, scale)
            
            volume = random.randint(5, 200) * 10000
            
            f.write(f"{timestamp_str},{bid:.{scale}f},{ask:.{scale}f},{volume}\n")



if __name__ == "__main__":
    data_dir = "../../../../data"
    generate_mock_csv("EURUSD", f"{data_dir}/DAT_ASCII_EURUSD_T_202601.csv", 6, 1.0850, num_ticks=1500000)
    generate_mock_csv("AUDUSD", f"{data_dir}/DAT_ASCII_AUDUSD_T_202601.csv", 6, 0.6620, num_ticks=1500000)
    generate_mock_csv("GBPUSD", f"{data_dir}/DAT_ASCII_GBPUSD_T_202601.csv", 6, 1.2740, num_ticks=1500000)
    generate_mock_csv("USDCHF", f"{data_dir}/DAT_ASCII_USDCHF_T_202601.csv", 6, 0.8910, num_ticks=1500000)
    generate_mock_csv("USDJPY", f"{data_dir}/DAT_ASCII_USDJPY_T_202601.csv", 3, 156.20, num_ticks=1500000)

    print("[+] Synthetic market dataset serialization successfully finalized.")