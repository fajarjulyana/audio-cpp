# 🎸 Tuner Gitar Chromatic CLI

Tuner gitar chromatic berbasis Command Line Interface (CLI) dengan live audio, menggunakan **C++**, **PortAudio**, dan **FFTW3** untuk analisis frekuensi.

## 🚀 Fitur
- **Deteksi Nada Otomatis**: Mengenali nada gitar berdasarkan frekuensi input.
- **FFT (Fast Fourier Transform)**: Menggunakan FFTW3 untuk analisis spektrum.
- **Real-Time Audio Processing**: Menggunakan PortAudio untuk menangkap input mikrofon secara langsung.
- **CLI Interface Sederhana**: Menampilkan frekuensi terdeteksi dan nada yang paling mendekati.

## 🛠️ Instalasi
### **1. Install Dependensi**
Sebelum mengompilasi, pastikan dependensi berikut sudah terinstal:
```bash
sudo apt update
sudo apt install libasound-dev autoconf libtool libfftw3-dev
```

Jika **PortAudio** tidak tersedia di repositori, install dari source:
```bash
wget http://www.portaudio.com/archives/pa_stable_v190700_20210406.tgz
tar xvf pa_stable_v190700_20210406.tgz
cd portaudio
./configure && make
sudo make install
```

### **2. Clone Repository**
```bash
git clone https://github.com/username/tuner-cli.git
cd tuner-cli
```

### **3. Compile Kode**
```bash
g++ -o tuner tuner.cpp -lportaudio -lfftw3 -lm -lasound -lpthread
```

### **4. Jalankan Program**
```bash
./tuner
```

## 🎛️ Penggunaan
1. Jalankan `./tuner` dan pastikan input mikrofon aktif.
2. Mainkan nada pada gitar, program akan menampilkan frekuensi dan nama not.
3. Tekan **CTRL+C** untuk keluar.

## 📜 Lisensi
Proyek ini dilisensikan di bawah [MIT License](LICENSE).

---
Dikembangkan oleh [PT Arvin Studio](https://arvin-plugin.github.io/) 🎶

