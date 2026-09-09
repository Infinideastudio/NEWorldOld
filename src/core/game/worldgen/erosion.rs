const P_ERODE: i32 = 4;

fn clamp(v: i32, n: usize) -> usize {
    v.max(0).min(n as i32 - 1) as usize
}

pub fn smooth_spike(x: f32) -> f32 {
    let a = -0.25;
    let k = 40.0;
    (1.0 + (k * (x - a / 2.0)).tanh()) / 2.0 - (-2.0 * k * (x - a).powi(2)).exp()
}

#[allow(clippy::needless_range_loop)]
pub fn erode(input: &[Vec<f32>], steps: usize) -> Vec<Vec<f32>> {
    let n = input.len();
    let mut map = input.to_vec();
    let mut water = vec![vec![0.0; n]; n];
    let mut weights = vec![[0.0; 8]; n * n];
    let diagonal = 2.0_f32.sqrt();
    for i in 0..n {
        for j in 0..n {
            let h = map[i][j];
            let neighbours = [
                (clamp(i as i32 - 1, n), clamp(j as i32 - 1, n), diagonal),
                (clamp(i as i32, n), clamp(j as i32 - 1, n), 1.0),
                (clamp(i as i32 + 1, n), clamp(j as i32 - 1, n), diagonal),
                (clamp(i as i32 + 1, n), clamp(j as i32, n), 1.0),
                (clamp(i as i32 + 1, n), clamp(j as i32 + 1, n), diagonal),
                (clamp(i as i32, n), clamp(j as i32 + 1, n), 1.0),
                (clamp(i as i32 - 1, n), clamp(j as i32 + 1, n), diagonal),
                (clamp(i as i32 - 1, n), clamp(j as i32, n), 1.0),
            ];
            let mut sum = 0.0;
            for (d, &(x, z, divisor)) in neighbours.iter().enumerate() {
                weights[i * n + j][d] = ((h - map[x][z]) / divisor).max(0.0).powi(P_ERODE);
                sum += weights[i * n + j][d];
            }
            if sum > 0.0 {
                for weight in &mut weights[i * n + j] {
                    *weight /= sum;
                }
            }
        }
    }
    for _ in 0..steps {
        let old = water.clone();
        for i in 0..n {
            for j in 0..n {
                let incoming = [
                    (i as i32 - 1, j as i32 - 1, 4),
                    (i as i32, j as i32 - 1, 5),
                    (i as i32 + 1, j as i32 - 1, 6),
                    (i as i32 + 1, j as i32, 7),
                    (i as i32 + 1, j as i32 + 1, 0),
                    (i as i32, j as i32 + 1, 1),
                    (i as i32 - 1, j as i32 + 1, 2),
                    (i as i32 - 1, j as i32, 3),
                ];
                water[i][j] = 1.0;
                for &(x, z, d) in &incoming {
                    water[i][j] +=
                        weights[clamp(x, n) * n + clamp(z, n)][d] * old[clamp(x, n)][clamp(z, n)];
                }
            }
        }
    }
    for i in 0..n {
        for j in 0..n {
            map[i][j] -= 0.004 * smooth_spike(map[i][j]) * water[i][j].ln();
        }
    }
    map
}

#[cfg(test)]
mod tests {
    use super::erode;

    #[test]
    fn erosion_changes_a_sloped_heightmap() {
        let input: Vec<Vec<f32>> = (0..8)
            .map(|z| (0..8).map(|x| (x + z) as f32 / 8.0).collect())
            .collect();
        let output = erode(&input, 20);
        assert!(output.iter().zip(&input).any(|(out_row, in_row)| {
            out_row
                .iter()
                .zip(in_row)
                .any(|(out, original)| (out - original).abs() > 1e-6)
        }));
    }
}
